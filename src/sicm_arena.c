#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/time.h>
#include <numaif.h>
#include <pthread.h>
#include <errno.h>
#include <sicm_low.h>
#include "sicmimpl.h"

static pthread_mutex_t sarenas_mutex = PTHREAD_MUTEX_INITIALIZER;
static int sarenas_num;
static sarena *sarenas_list;
static size_t sarenas_lookup_mib[2];

static extent_hooks_t sarena_hooks;

// called with sarenas_mutex held
int sicm_arena_init() {
	int err;
	size_t miblen;

	miblen = 2;
	err = je_mallctlnametomib("arenas.lookup", sarenas_lookup_mib, &miblen);
	if (err != 0) {
		fprintf(stderr, "can't get mib: %d\n", err);
		return -1;
	}

	return 0;
}

sicm_arena sicm_arena_create(size_t sz, sicm_device *dev) {
	int err;
	sarena *sa;
	char buf[32];
	size_t arena_ind_sz;
	size_t hooks_mib[3];
	size_t hooks_miblen;
	size_t old_size, new_size;
	extent_hooks_t *new_hooks, *old_hooks;
	unsigned arena_ind;

	sa = malloc(sizeof(sarena));
	if (sa == NULL)
		return NULL;

	pthread_mutex_init(&sa->mutex, NULL);
	sa->dev = dev;
	sa->size = sz;
	sa->rangesz = 4;
	sa->rangenum = 0;
	sa->ranges = calloc(sa->rangesz, sizeof(srange));
	if (sa->ranges == NULL) {
error:
		free(sa);
		return NULL;
	}

	sa->hooks = sarena_hooks;
	arena_ind_sz = sizeof(unsigned); // sa->arena_ind);
	arena_ind = -1;
	err = je_mallctl("arenas.create", (void *) &arena_ind, &arena_ind_sz, NULL, 0);
	if (err != 0) {
		fprintf(stderr, "can't create an arena: %d\n", err);
		goto error;
	}

	sa->arena_ind = arena_ind;
	snprintf(buf, sizeof(buf), "arena.%d.extent_hooks", sa->arena_ind);
	hooks_miblen = 3;
	err = je_mallctlnametomib(buf, hooks_mib, &hooks_miblen);
	if (err != 0) {
		fprintf(stderr, "can't get mib: %d\n", err);
		goto error;
	}

	old_size = sizeof(old_hooks);
	new_size = old_size;
	new_hooks = &sa->hooks;

	err = je_mallctlbymib(hooks_mib, hooks_miblen, (void *)&old_hooks, &old_size, (void *)&new_hooks, new_size);
	if (err != 0) {
		fprintf(stderr, "can't setup hooks: %d\n", err);
		goto error;
	}

	// add the arena to the global list of arenas
	pthread_mutex_lock(&sarenas_mutex);
	sa->next = sarenas_list;
	sarenas_list = sa;
	sarenas_num++;
	pthread_mutex_unlock(&sarenas_mutex);

	return sa;
}

sicm_arena_list *sicm_arenas_list() {
	int i;
	sicm_arena_list *l;
	sarena *a;

	pthread_mutex_lock(&sarenas_mutex);
	l = malloc(sizeof(sicm_arena_list) + sarenas_num * sizeof(sicm_arena));
	l->arenas = (sicm_arena *) &l[1];
	for(i = 0, a = sarenas_list; i < sarenas_num && a != NULL; i++, a = a->next) {
		l->arenas[i] = a;
	}
	l->count = i;

	return l;
}

sicm_device *sicm_arena_get_device(sicm_arena a) {
	sicm_device *ret;
	sarena *sa;

	sa = a;
	ret = NULL;
	if (sa != NULL) {
		pthread_mutex_lock(&sa->mutex);
		ret = sa->dev;
		pthread_mutex_unlock(&sa->mutex);
	}

	return ret;
}

int sicm_arena_set_device(sicm_arena a, sicm_device *dev) {
	int i, err, node, maxnode;
	unsigned long nodemask;
	sarena *sa;
	srange *r;

	sa = a;
	if (sa == NULL)
		return -EINVAL;

	err = 0;
	node = sicm_numa_id(dev);
	nodemask = 1 << node;
	maxnode = 32; // FIXME
	pthread_mutex_lock(&sa->mutex);
	for(i = 0; i < sa->rangenum; i++) {
		r = &sa->ranges[i];
		err = mbind((void *) r->addr, r->size, MPOL_BIND, &nodemask, maxnode, MPOL_MF_MOVE);
		if (err < 0)
			// TODO: move back the extents on error?
			goto out;
	}
	sa->dev = dev;

out:
	pthread_mutex_unlock(&sa->mutex);
	return err;
}

size_t sicm_arena_size(sicm_arena a) {
	size_t ret;
	sarena *sa;

	sa = a;
	pthread_mutex_lock(&sa->mutex);
	ret = sa->size;
	pthread_mutex_unlock(&sa->mutex);

	return ret;
}

void *sicm_arena_alloc(sicm_arena a, size_t sz) {
	sarena *sa;
	int flags;
	void *ret;

	sa = a;
	flags = 0;
	if (sa != NULL)
		flags = MALLOCX_ARENA(sa->arena_ind) | MALLOCX_TCACHE_NONE;

	ret = je_mallocx(sz, flags);
	return ret;
}

void sicm_free(void *ptr) {
	je_free(ptr);
}

void *sicm_realloc(void *ptr, size_t sz) {
	// TODO: should we include MALLOCX_ARENA(...)???
	return je_rallocx(ptr, sz, MALLOCX_TCACHE_NONE);
}

sarena *sarena_ptr2sarena(void *ptr) {
	int err;
	unsigned arena_ind;
	size_t ai_sz;
	sarena *sa;

	sa = NULL;
	ai_sz = sizeof(unsigned);
	err = je_mallctlbymib(sarenas_lookup_mib, 2, &arena_ind, &ai_sz, &ptr, sizeof(ptr));
        if (err != 0) {
                fprintf(stderr, "can't setup hooks: %d\n", err);
                goto out;
        }

	// TODO: make this lookup faster if this becomes bottleneck
	pthread_mutex_lock(&sarenas_mutex);
	for(sa = sarenas_list; sa != NULL; sa = sa->next) {
		if (sa->arena_ind == arena_ind)
			break;
	}
	pthread_mutex_unlock(&sarenas_mutex);

out:
	return sa;
}

sicm_arena sicm_lookup(void *ptr) {
	return sarena_ptr2sarena(ptr);
}

static void *sa_alloc(extent_hooks_t *, void *, size_t, size_t, bool *, bool *, unsigned);
static bool sa_dalloc(extent_hooks_t *, void *, size_t, bool, unsigned);
static void sa_destroy(extent_hooks_t *, void *, size_t, bool, unsigned);
static bool sa_commit(extent_hooks_t *, void *, size_t, size_t, size_t, unsigned);
static bool sa_decommit(extent_hooks_t *, void *, size_t, size_t, size_t, unsigned);
static bool sa_split(extent_hooks_t *, void *, size_t, size_t, size_t, bool, unsigned);
static bool sa_merge(extent_hooks_t *, void *, size_t, void *, size_t, bool, unsigned);

static extent_hooks_t sarena_hooks = {
	.alloc = sa_alloc,
	.dalloc = sa_dalloc,
	.destroy = sa_destroy,
	.commit = sa_commit,
	.decommit = sa_decommit,
	.purge_lazy = NULL,
	.purge_forced = NULL,
	.split = sa_split,
	.merge = sa_merge,
};

static void *sa_alloc(extent_hooks_t *h, void *new_addr, size_t size, size_t alignment, bool *zero, bool *commit, unsigned arena_ind) {
	sarena *sa;
	uintptr_t n, m, maxnode;
	int oldmode;
	unsigned long nodemask, oldnodemask;
	void *ret;
	size_t sasize, maxsize;

	*commit = 1;
	*zero = 1;
	sa = container_of(h, sarena, hooks);

//	printf("sa_alloc: sa %p new_addr %p size %lx alignment %lx sa->arena_ind %d arena_ind %d\n", sa, new_addr, size, alignment, sa->arena_ind, arena_ind);

	// TODO: figure out a way to prevent taking the mutex twice (sa_range_add also takes it)...
	pthread_mutex_lock(&sa->mutex);
	sasize = sa->size;
	maxsize = sa->maxsize;
	pthread_mutex_unlock(&sa->mutex);
	if (maxsize > 0 && sasize + size > maxsize)
		return NULL;

	maxnode = 32; // sa->node + 1;
	get_mempolicy(&oldmode, &oldnodemask, maxnode, NULL, 0);
        nodemask = 1 << sicm_numa_id(sa->dev);
        if (set_mempolicy(MPOL_BIND, &nodemask, maxnode) < 0) {
                perror("set_mempolicy");
                return NULL;
        }

	ret = mmap(new_addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_POPULATE, -1, 0);
	if (ret == MAP_FAILED)
		return NULL;
 
	if (alignment == 0 || ((uintptr_t) ret)%alignment == 0)
		// we are lucky and got the right alignment
		goto success;

	// the alignment didn't work out, munmap and try again
	munmap(ret, size);

	// if new_addr is set, we can't fulful the alignment, so just fail
	if (new_addr != NULL) {
error:
		set_mempolicy(oldmode, &oldnodemask, maxnode);
		return NULL;
	}

	size += alignment;
	ret = mmap(NULL, size + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_POPULATE, -1, 0);
	if (ret == MAP_FAILED)
		goto error;

	n = (uintptr_t) ret;
	m = n + alignment - (n%alignment);
	munmap(ret, m-n);
	ret = (void *) m;

success:
	set_mempolicy(oldmode, &oldnodemask, maxnode);
	sa_range_add(sa, (uintptr_t) ret, size);
	return ret;
}

static bool sa_dalloc(extent_hooks_t *h, void *addr, size_t size, bool committed, unsigned arena_ind) {
	sarena *sa;

	sa = container_of(h, sarena, hooks);
	printf("sa_dalloc: sa %p addr %p size %ld sa->arena_ind %d arena_ind %d\n", sa, addr, size, sa->arena_ind, arena_ind);
	if (sa_range_del(sa, (uintptr_t) addr, size) != 0) {
		fprintf(stderr, "extent not found: %p %ld\n", addr, size);
		return true;
	}

	if (munmap(addr, size) != 0) {
		fprintf(stderr, "munmap failed: %p %ld\n", addr, size);
		return true;
	}

	return false;
}

static void sa_destroy(extent_hooks_t *h, void *addr, size_t size, bool committed, unsigned arena_ind) {
	sa_dalloc(h, addr, size, committed, arena_ind);
}

static bool sa_commit(extent_hooks_t *h, void *addr, size_t size, size_t offset, size_t length, unsigned arena_ind) {
	// no support for commit/decommit
	return false;
}

static bool sa_decommit(extent_hooks_t *h, void *addr, size_t size, size_t offset, size_t length, unsigned arena_ind) {
	// no support for commit/decommit
	return true;
}

static bool sa_split(extent_hooks_t *h, void *addr, size_t size, size_t size_a, size_t size_b, bool committed, unsigned arena_ind) {
	// we don't really need to keep track of each extent, let's figure it out at alloc/dalloc time
	return false;
}

static bool sa_merge(extent_hooks_t *h, void *addr_a, size_t size_a, void *addr_b, size_t size_b, bool committed, unsigned arena_ind) {
	// we don't really need to keep track of each extent, let's figure it all out at alloc/dalloc time
	return false;
}

// should be called with the sa lock held
static int sa_range_search(sarena *sa, uintptr_t addr) {
	int b, t, n;
	srange *r;

	n = 0;
	b = 0;
	t = sa->rangenum;
	while (b < t) {
		n = (b + t) / 2;
		r = &sa->ranges[n];
		if (r->addr == addr)
			break;
		else if (r->addr < addr) {
			b = n + 1;
			n++;
		} else
			t = n;
	}

	return n;
}

// makes space for one entry at index n
// should be called with the sa lock held
static int sa_range_make_space(sarena *sa, int n) {
	srange *tmp;

	if (sa->rangesz == sa->rangenum) {
		sa->rangesz += 16;
		tmp = realloc(sa->ranges, sa->rangesz * sizeof(srange));
		if (tmp == NULL) {
			fprintf(stderr, "realloc failed\n");
			return -1;
		}

		sa->ranges = tmp;
	}

	if (n != sa->rangenum)
		memmove((void *) &sa->ranges[n+1], (void *) &sa->ranges[n], (sa->rangenum - n) * sizeof(srange));

	return 0;
}

int sa_range_add(sarena *sa, uintptr_t addr, size_t size) {
	int n, m, ret, fcombine;
	srange *r0, *r1;

	ret = 0;
	r0 = NULL;
	r1 = NULL;
	pthread_mutex_lock(&sa->mutex);
	n = sa_range_search(sa, addr);

//	printf("sa_range_add: %lx:%lx n %d\n", addr, size, n);
	// ensure that the extent doesn't overlap with the previous or the next extent (just in case)
	if (n > 0) {
		r0 = &sa->ranges[n-1];
		if (r0->addr + r0->size > addr) {
			fprintf(stderr, "overlapping existing range: %lx:%lx with %lx:%lx\n", r0->addr, r0->size, addr, size);
			ret = -1;
			goto out;
		}
	}

	if (n < sa->rangenum) {
		r1 = &sa->ranges[n];
		if (addr + size > r1->addr) {
			fprintf(stderr, "overlapping existing range: %lx:%lx with %lx:%lx\n", r1->addr, r1->size, addr, size);
			ret = -1;
			goto out;
		}
	}

	// try to combine ranges
	m = 1;
	fcombine = 0;
	if (r0 != NULL && r0->addr + r0->size == addr) {
		// we can combine with the previous range
		m--;
		fcombine = 1;
	}

	if (r1 != NULL && addr+size == r1->addr) {
		// we can combine with the next range
		m--;
	}

	switch (m) {
	case -1:
		// we can combine both the previous and the next ranges
//		printf("\tcombine both\n");
		r0->size += size + r1->size;
		if (n < sa->rangenum)
			memmove(&sa->ranges[n], &sa->ranges[n+1], (sa->rangenum - n - 1) * sizeof(srange));
		sa->rangenum--;
		break;

	case 0:
		// we can combine one of the existing ranges
//		printf("\tcombine one\n");
		if (fcombine) {
			r0->size += size;
		} else {
			r1->addr = addr;
			r1->size += size;
		}
		break;

	case 1:
//		printf("\tcan't combine\n");
		// we can't combine any ranges
		sa_range_make_space(sa, n);
		sa->ranges[n].addr = addr;
		sa->ranges[n].size = size;
		sa->rangenum++;
		break;
	}

	sa->size += size;

out:
	pthread_mutex_unlock(&sa->mutex);
	return ret;
}

int sa_range_del(sarena *sa, uintptr_t addr, size_t size) {
	int n, m, ret;
	srange *r;
	uintptr_t a1;
	size_t sz0, sz1;

	ret = 0;
	pthread_mutex_lock(&sa->mutex);
	n = sa_range_search(sa, addr);
	if (n >= sa->rangenum) {
		fprintf(stderr, "range not found: %lx:%lx\n", addr, size);
		ret = -1;
		goto out;
	}

	r = &sa->ranges[n];
	if (r->addr + r->size < addr) {
		fprintf(stderr, "range not found: %lx:%lx\n", addr, size);
		ret = -1;
		goto out;
	}

	if (r->addr + r->size < addr + size) {
		fprintf(stderr, "range (%lx:%lx) extends beyond the existing range (%lx:%lx)\n", addr, size, r->addr, r->size);
		ret = -1;
		goto out;
	}

	// calculate the ranges that we keep
	sz0 = addr - r->addr;
	a1 = addr + size;
	sz1 = r->addr + r->size - a1;

	// and the number of entries we have to remove/add to the array
	m = -1;
	if (sz0 != 0)
		m++;
	if (sz1 != 0)
		m++;

	switch (m) {
	case -1:
		// delete the whole entry
		if (n+1 < sa->rangenum)
			memmove(&sa->ranges[n], &sa->ranges[n+1], (sa->rangenum - n - 1) * sizeof(srange));
		sa->rangenum--;
		break;

	case 0:
		// there range is at one of the ends, we don't have to add or remove entries
		if (sz0 != 0) {
			r->size = sz0;
		} else {
			r->addr = a1;
			r->size = sz1;
		}
		break;

	case 1:
		// the range is in the middle, we need to add an entry and adjust the existing one
		if (sa_range_make_space(sa, n+1) < 0) {
			ret = -1;
			goto out;
		}

		r->size = sz0;
		sa->ranges[n+1].addr = a1;
		sa->ranges[n+1].size = sz1;
		sa->rangenum++;
		break;
	}

	sa->size -= size;

out:
	pthread_mutex_unlock(&sa->mutex);
	return ret;
	
}
