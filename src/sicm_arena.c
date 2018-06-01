#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/time.h>
#include <numa.h>
#include <numaif.h>
#include <pthread.h>
#include <errno.h>
#include <sicm_low.h>
#include "sicmimpl.h"
#include "rbtree.h"

static pthread_mutex_t sa_mutex = PTHREAD_MUTEX_INITIALIZER;
static int sa_num;
static sarena *sa_list;
static size_t sa_lookup_mib[2];
static pthread_once_t sa_init = PTHREAD_ONCE_INIT;
static pthread_key_t sa_default_key;
static extent_hooks_t sa_hooks;

static void sarena_init() {
	int err;
	size_t miblen;

	pthread_key_create(&sa_default_key, NULL);
	miblen = 2;
	err = je_mallctlnametomib("arenas.lookup", sa_lookup_mib, &miblen);
	if (err != 0)
		fprintf(stderr, "can't get mib: %d\n", err);
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

	pthread_once(&sa_init, sarena_init);
	sa = malloc(sizeof(sarena));
	if (sa == NULL)
		return NULL;

	pthread_mutex_init(&sa->mutex, NULL);
	sa->dev = dev;
	sa->size = sz;
	sa->pagesize = sicm_device_page_size(dev);
	sa->numaid = sicm_numa_id(dev);

	if (sa->numaid < 0) {
error:
		return NULL;
	}

	sa->ranges = sicm_create_tree();
	sa->hooks = sa_hooks;
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
	pthread_mutex_lock(&sa_mutex);
	sa->next = sa_list;
	sa_list = sa;
	sa_num++;
	pthread_mutex_unlock(&sa_mutex);

	return sa;
}

sicm_arena_list *sicm_arenas_list() {
	int i;
	sicm_arena_list *l;
	sarena *a;

	pthread_mutex_lock(&sa_mutex);
	l = malloc(sizeof(sicm_arena_list) + sa_num * sizeof(sicm_arena));
	l->arenas = (sicm_arena *) &l[1];
	for(i = 0, a = sa_list; i < sa_num && a != NULL; i++, a = a->next) {
		l->arenas[i] = a;
	}
	l->count = i;
	pthread_mutex_unlock(&sa_mutex);

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

// should be called with sa mutex held
static void sicm_arena_range_move(void *aux, void *start, void *end) {
	int err;
	sarena *sa = (sarena *) aux;
    struct bitmask *nodemask = numa_allocate_nodemask();
    numa_bitmask_setbit(nodemask, sa->numaid);

	err = mbind((void *) start, (char*) end - (char*) start, MPOL_BIND, nodemask->maskp, nodemask->size, MPOL_MF_MOVE | MPOL_MF_STRICT);
//	printf("sicm_arena_range_move %p %ld: %d\n", start, (char*)end - (char*)start, err);
	if (err < 0 && sa->err == 0)
		sa->err = err;

    numa_free_nodemask(nodemask);
}

// FIXME: doesn't support moving to huge pages
int sicm_arena_set_device(sicm_arena a, sicm_device *dev) {
	int err, node, oldnumaid;
	sarena *sa;

	sa = a;
	if (sa == NULL)
		return -EINVAL;

//	printf("sicm_arena_set_device: %p\n", sa);
	if (sa->pagesize != sicm_device_page_size(dev))
		return -EINVAL;

	err = 0;
	node = sicm_numa_id(dev);
	if (node < 0)
		return -EINVAL;

	pthread_mutex_lock(&sa->mutex);
	oldnumaid = sa->numaid;
	sa->numaid = node;
	sa->err = 0;
	sicm_map_tree(sa->ranges, sa, sicm_arena_range_move);
	if (sa->err) {
		// at least one extent wasn't moved, try to roll back them all
		err = sa->err;
		sa->numaid = oldnumaid;
		sa->err = 0;
		sicm_map_tree(sa->ranges, sa, sicm_arena_range_move);
		// TODO: not sure what to do if moving back fails
	} else {
		sa->dev = dev;
	}

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

void *sicm_alloc(size_t sz) {
	sarena *sa;
	void *ret;

	sa = pthread_getspecific(sa_default_key);
	if (sa != NULL)
		ret = sicm_arena_alloc(sa, sz);
	else
		ret = je_malloc(sz);

	return ret;
}

void sicm_free(void *ptr) {
	je_free(ptr);
}

void *sicm_realloc(void *ptr, size_t sz) {
	// TODO: should we include MALLOCX_ARENA(...)???
	return je_rallocx(ptr, sz, MALLOCX_TCACHE_NONE);
}

void sicm_arena_set_default(sicm_arena sa) {
	pthread_setspecific(sa_default_key, sa);
}

sicm_arena sicm_arena_get_default(void) {
	sicm_arena *sa;

	sa = pthread_getspecific(sa_default_key);
	return sa;
}

sarena *sarena_ptr2sarena(void *ptr) {
	int err;
	unsigned arena_ind;
	size_t ai_sz;
	sarena *sa;

	sa = NULL;
	ai_sz = sizeof(unsigned);
	err = je_mallctlbymib(sa_lookup_mib, 2, &arena_ind, &ai_sz, &ptr, sizeof(ptr));
        if (err != 0) {
                fprintf(stderr, "can't setup hooks: %d\n", err);
                goto out;
        }

	// TODO: make this lookup faster if this becomes bottleneck
	pthread_mutex_lock(&sa_mutex);
	for(sa = sa_list; sa != NULL; sa = sa->next) {
		if (sa->arena_ind == arena_ind)
			break;
	}
	pthread_mutex_unlock(&sa_mutex);

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

static extent_hooks_t sa_hooks = {
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
	uintptr_t n, m;
	int oldmode;
	void *ret;
	size_t sasize, maxsize;
    struct bitmask *nodemask;
    struct bitmask *oldnodemask;

	*commit = 1;
	*zero = 1;
	sa = container_of(h, sarena, hooks);

//	printf("sa_alloc: sa %p new_addr %p size %lx alignment %lx sa->arena_ind %d arena_ind %d\n", sa, new_addr, size, alignment, sa->arena_ind, arena_ind);

	// TODO: figure out a way to prevent taking the mutex twice (sa_range_add also takes it)...
	pthread_mutex_lock(&sa->mutex);
	sasize = sa->size;
	maxsize = sa->maxsize;
	pthread_mutex_unlock(&sa->mutex);
    if (maxsize > 0 && sasize + size > maxsize) {
		return NULL;
    }

    nodemask = numa_allocate_nodemask();
    oldnodemask = numa_allocate_nodemask();

	get_mempolicy(&oldmode, oldnodemask->maskp, oldnodemask->size, NULL, 0);
    numa_bitmask_setbit(nodemask, sa->numaid);
    if (set_mempolicy(MPOL_BIND, nodemask->maskp, nodemask->size) < 0) {
        perror("set_mempolicy");
        numa_free_nodemask(nodemask);
        numa_free_nodemask(oldnodemask);
        return NULL;
    }

	ret = mmap(new_addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_POPULATE, -1, 0);
	if (ret == MAP_FAILED)
		goto error;

	if (alignment == 0 || ((uintptr_t) ret)%alignment == 0)
		// we are lucky and got the right alignment
		goto success;

	// the alignment didn't work out, munmap and try again
	munmap(ret, size);

	// if new_addr is set, we can't fulful the alignment, so just fail
	if (new_addr != NULL) {
error:
		set_mempolicy(oldmode, oldnodemask->maskp, oldnodemask->size);
        numa_free_nodemask(nodemask);
        numa_free_nodemask(oldnodemask);
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
	set_mempolicy(oldmode, oldnodemask->maskp, oldnodemask->size);
    numa_free_nodemask(nodemask);
    numa_free_nodemask(oldnodemask);
	pthread_mutex_lock(&sa->mutex);
	sicm_insert(sa->ranges, ret, (char *)ret + size);
	pthread_mutex_unlock(&sa->mutex);
	return ret;
}

static bool sa_dalloc(extent_hooks_t *h, void *addr, size_t size, bool committed, unsigned arena_ind) {
	sarena *sa;
	bool ret;

	ret = false;
	sa = container_of(h, sarena, hooks);
//	printf("sa_dalloc: sa %p addr %p size %ld sa->arena_ind %d arena_ind %d\n", sa, addr, size, sa->arena_ind, arena_ind);
	pthread_mutex_lock(&sa->mutex);
	sicm_delete(sa->ranges, addr, (char *)addr + size);
	if (munmap(addr, size) != 0) {
		fprintf(stderr, "munmap failed: %p %ld\n", addr, size);
		sicm_insert(sa->ranges, addr, (char *)addr + size);
		ret = true;
	}
	pthread_mutex_unlock(&sa->mutex);
	return ret;
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
