#include <errno.h>
#include <math.h>
#include <numa.h>
#include <numaif.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include "sicm_low.h"
#include "sicm_impl.h"

static pthread_mutex_t sa_mutex = PTHREAD_MUTEX_INITIALIZER;
static int sa_num;
static sarena *sa_list;
static size_t sa_lookup_mib[2];
static pthread_once_t sa_init = PTHREAD_ONCE_INIT;
static pthread_key_t sa_default_key;
static extent_hooks_t sa_hooks;
void (*sicm_extent_alloc_callback)(sarena *arena, void *start, void *end) = NULL;
void (*sicm_extent_dalloc_callback)(sarena *arena, void *start, void *end) = NULL;

static void sarena_init() {
	int err;
	size_t miblen;

	pthread_key_create(&sa_default_key, NULL);
	miblen = 2;
	err = je_mallctlnametomib("arenas.lookup", sa_lookup_mib, &miblen);
	if (err != 0)
		fprintf(stderr, "can't get mib: %d\n", err);
}

// check if all devices use NUMA and if they are have the same page size
static struct bitmask *sicm_device_list_check_numa(sicm_device_list *devs) {
	int i, cpgsz;
	struct bitmask *nodemask;

	cpgsz = -1;
	nodemask = numa_allocate_nodemask();
	for(i = 0; i < devs->count; i++) {
		int numaid, pgsz;

		numaid = sicm_numa_id(devs->devices[i]);
		pgsz = sicm_device_page_size(devs->devices[i]);
		if (numaid < 0)
			goto error;

		if (cpgsz != pgsz) {
			if (cpgsz == -1)
				cpgsz = pgsz;
			else
				goto error;
		}

		numa_bitmask_setbit(nodemask, numaid);
	}

	return nodemask;

error:
	numa_free_nodemask(nodemask);
	return NULL;
}

static sarena *sicm_arena_new(size_t sz, sicm_arena_flags flags, sicm_device_list *devs, int fd, off_t offset, int mutexfd, off_t mutexoff) {
	int err, cpgsz;
	sarena *sa;
	size_t arena_ind_sz;
	extent_hooks_t *new_hooks;
	unsigned arena_ind;
	pthread_mutexattr_t attr;
	struct bitmask *nodemask;

	pthread_once(&sa_init, sarena_init);

	nodemask = sicm_device_list_check_numa(devs);
	if (nodemask == NULL)
		return NULL;

	
	sa = malloc(sizeof(sarena));
	if (sa == NULL) {
		return NULL;
	}

	sa->flags = flags;
	sa->devs.count = devs->count;
	sa->devs.devices = malloc(devs->count * sizeof(sicm_device *));
	if (sa->devs.devices == NULL) {
		free(sa);
		return NULL;
	}
	memcpy(sa->devs.devices, devs->devices, devs->count * sizeof(sicm_device *));

	sa->mutex = (pthread_mutex_t *) mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, mutexfd==-1?MAP_PRIVATE | MAP_ANONYMOUS:MAP_SHARED, mutexfd, mutexoff);
	if (sa->mutex == MAP_FAILED) {
		perror("what?");
		free(sa->devs.devices);
		free(sa);
		return NULL;
	}

	pthread_mutexattr_init(&attr);
	if (mutexfd >= 0)
		pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

	pthread_mutex_init(sa->mutex, &attr);
	sa->size = 0;
	sa->maxsize = sz;
	sa->nodemask = nodemask;
	sa->fd = -1;	// DON'T TOUCH! sa_alloc depends on it being -1 when arenas.crete is called.
	sa->extents = extent_arr_init();
	sa->hooks = sa_hooks;
	new_hooks = &sa->hooks;
	arena_ind_sz = sizeof(unsigned); // sa->arena_ind);
	arena_ind = -1;
	err = je_mallctl("arenas.create", (void *) &arena_ind, &arena_ind_sz, (void *)&new_hooks, sizeof(extent_hooks_t *));
	if (err != 0) {
		fprintf(stderr, "can't create an arena: %d\n", err);
		pthread_mutex_destroy(sa->mutex);
		munmap(sa->mutex, sizeof(pthread_mutex_t));
		free(sa);
		return NULL;
	}

	sa->arena_ind = arena_ind;

	// DON'T MOVE THESE TWO ASSIGNMENTS UP!
	// The jemalloc code needs to allocate an extent or two for internal
	// use and our extent allocation code checks if sa->fd is negative
	// to decide whether to allocate private or shared region
	sa->size = offset;	// FIXME: is this correct???
	sa->fd = fd;

	// add the arena to the global list of arenas
	pthread_mutex_lock(&sa_mutex);
	sa->next = sa_list;
	sa_list = sa;
	sa_num++;
	pthread_mutex_unlock(&sa_mutex);

	return sa;
}

sicm_arena sicm_arena_create(size_t sz, sicm_arena_flags flags, sicm_device_list *devs) {
	return sicm_arena_new(sz, flags, devs, -1, 0, -1, 0);
}

sicm_arena sicm_arena_create_mmapped(size_t sz, sicm_arena_flags flags, sicm_device_list *devs, int fd,
						off_t offset, int mutex_fd, off_t mutex_offset) {
	return sicm_arena_new(sz, flags, devs, fd, offset, mutex_fd, mutex_offset);
}

void sicm_arena_destroy(sicm_arena arena) {
	sarena *sa = arena;
	char str[32];
	size_t arena_ind_sz;

	if (sa == NULL)
		return;

	/* Free up the arena */
	snprintf(str, sizeof(str), "arena.%u.destroy", sa->arena_ind);
	arena_ind_sz = sizeof(unsigned);
	je_mallctl(str, (void *) &sa->arena_ind, &arena_ind_sz, NULL, 0);

	extent_arr_free(sa->extents);
	munmap(sa->mutex, sizeof(pthread_mutex_t));
	free(sa->devs.devices);
	numa_free_nodemask(sa->nodemask);
	free(sa);
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
	free(l);
	pthread_mutex_unlock(&sa_mutex);

	return l;
}

sicm_device_list sicm_arena_get_devices(sicm_arena a) {
	sarena *sa;
	sicm_device_list ret;

	ret.count = 0;
	ret.devices = NULL;
	sa = a;
	if (sa != NULL) {
		pthread_mutex_lock(sa->mutex);
		ret.count = sa->devs.count;
		ret.devices = malloc(ret.count * sizeof(sicm_device*));
		memcpy(ret.devices, sa->devs.devices, ret.count * sizeof(sicm_device*));
		pthread_mutex_unlock(sa->mutex);
	}

	return ret;
}

// should be called with sa mutex held
static void sicm_arena_range_move(void *aux, void *start, void *end) {
	int err;
	int mpol;
	unsigned long *nodemaskp, maxnode;
  unsigned flags;
	sarena *sa = (sarena *) aux;

	switch (sa->flags & SICM_ALLOC_MASK) {
	case SICM_ALLOC_STRICT:
		mpol = MPOL_BIND;
		nodemaskp = sa->nodemask->maskp;
		maxnode = sa->nodemask->size;
		break;

	case SICM_ALLOC_RELAXED:
		// TODO: this will work only for single device, fix it
		mpol = MPOL_PREFERRED;
		nodemaskp = sa->nodemask->maskp;
		maxnode = sa->nodemask->size;
		break;

	default:
		mpol = MPOL_DEFAULT;
		nodemaskp = NULL;
		maxnode = 0;
		break;
	}

  flags = 0;
  if (!(sa->flags & SICM_MOVE_RELAXED)) {
    printf("Blocking until pages are moved\n");
    flags = MPOL_MF_MOVE;
  } else {
    printf("Moving pages in a relaxed manner\n");
  }

	err = mbind((void *) start, (char*) end - (char*) start, mpol, nodemaskp, maxnode, flags);
	if (err < 0 && sa->err == 0)
		sa->err = err;
}

// FIXME: doesn't support moving to huge pages
int sicm_arena_set_devices(sicm_arena a, sicm_device_list *devs) {
	int err, node, oldnumaid;
	size_t i;
	sarena *sa;
	struct bitmask *nodemask, *oldnodemask;

	sa = a;
	if (sa == NULL)
		return -EINVAL;

	nodemask = sicm_device_list_check_numa(devs);
	if (nodemask == NULL)
		return -EINVAL;

	if (sicm_device_page_size(devs->devices[0]) != sicm_device_page_size(sa->devs.devices[0]))
		return -EINVAL;

	err = 0;
	pthread_mutex_lock(sa->mutex);
	oldnodemask = sa->nodemask;
	sa->nodemask = nodemask;
	sa->err = 0;
	extent_arr_for(sa->extents, i) {
		if(!sa->extents->arr[i].start && !sa->extents->arr[i].end) continue;
		sicm_arena_range_move(sa, sa->extents->arr[i].start, sa->extents->arr[i].end);
	}

	if (sa->err) {
		// at least one extent wasn't moved, try to roll back the ones that succeeded
		err = sa->err;
		sa->nodemask = oldnodemask;
		sa->err = 0;
		extent_arr_for(sa->extents, i) {
			if(!sa->extents->arr[i].start && !sa->extents->arr[i].end) continue;
			sicm_arena_range_move(sa, sa->extents->arr[i].start, sa->extents->arr[i].end);
		}
		// TODO: not sure what to do if moving back fails
		numa_free_nodemask(nodemask);
		return sa->err;
	} else {
		sa->devs.count = devs->count;
		sa->devs.devices = realloc(sa->devs.devices, devs->count * sizeof(sicm_device *));
		memcpy(sa->devs.devices, devs->devices, devs->count * sizeof(sicm_device *));
		numa_free_nodemask(oldnodemask);
	}

	pthread_mutex_unlock(sa->mutex);

	return err;
}

size_t sicm_arena_size(sicm_arena a) {
	size_t ret;
	sarena *sa;

	sa = a;
	pthread_mutex_lock(sa->mutex);
	ret = sa->size;
	pthread_mutex_unlock(sa->mutex);

	return ret;
}

void *sicm_arena_alloc(sicm_arena a, size_t sz) {
	sarena *sa;
	int flags;

	if (sz == 0) {
		return je_malloc(0);
	}

	sa = a;
	flags = 0;
	if (sa != NULL) {
		flags = MALLOCX_ARENA(sa->arena_ind) | MALLOCX_TCACHE_NONE;
	}

	return je_mallocx(sz, flags);
}

void *sicm_arena_alloc_aligned(sicm_arena a, size_t sz, size_t align) {
	sarena *sa;
	int flags;

	sa = a;
	flags = 0;
	if (sa != NULL) {
    flags = MALLOCX_ALIGN(align) | MALLOCX_ARENA(sa->arena_ind) | MALLOCX_TCACHE_NONE;
  }

	return je_mallocx(sz, flags);
}

void *sicm_arena_realloc(sicm_arena a, void *ptr, size_t sz) {
	sarena *sa;
	int flags;

	if (sz == 0) {
		sicm_free(ptr);
		return NULL;
	}

	sa = a;
	flags = 0;
	if (sa != NULL)
		flags = MALLOCX_ARENA(sa->arena_ind) | MALLOCX_TCACHE_NONE;

	return je_rallocx(ptr, sz, flags);
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

void *sicm_alloc_aligned(size_t sz, size_t align) {
	sarena *sa;
	void *ret;

	sa = pthread_getspecific(sa_default_key);
	if (sa != NULL)
		ret = sicm_arena_alloc_aligned(sa, sz, align);
	else
		ret = je_aligned_alloc(align, sz);

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

sicm_arena sicm_arena_lookup(void *ptr) {
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
	int mpol;
	unsigned long *nodemaskp, maxnode;
	sarena *sa;
	uintptr_t n, m;
	int oldmode, mmflags;
	void *ret;
	struct bitmask *oldnodemask;

	*commit = 1;
	*zero = 0;
	ret = NULL;
	sa = container_of(h, sarena, hooks);

	// TODO: figure out a way to prevent taking the mutex twice (sa_range_add also takes it)...
	pthread_mutex_lock(sa->mutex);
	if (sa->maxsize > 0 && sa->size + size > sa->maxsize) {
		return NULL;
	}

	oldnodemask = numa_allocate_nodemask();
	get_mempolicy(&oldmode, oldnodemask->maskp, oldnodemask->size, NULL, 0);
	switch (sa->flags & SICM_ALLOC_MASK) {
	case SICM_ALLOC_STRICT:
		mpol = MPOL_BIND;
		nodemaskp = sa->nodemask->maskp;
		maxnode = sa->nodemask->size;
		break;

	case SICM_ALLOC_RELAXED:
		// TODO: this will work only for single device, fix it
		mpol = MPOL_PREFERRED;
		nodemaskp = sa->nodemask->maskp;
		maxnode = sa->nodemask->size;
		break;

	default:
		mpol = MPOL_DEFAULT;
		nodemaskp = NULL;
		maxnode = 0;
		break;
	}

	if (set_mempolicy(mpol, nodemaskp, maxnode) < 0) {
		perror("set_mempolicy");
		goto free_nodemasks;
	}

	if (sa->fd == -1)
		mmflags = MAP_ANONYMOUS|MAP_PRIVATE;
	else
		mmflags = MAP_SHARED;

	ret = mmap(new_addr, size, PROT_READ | PROT_WRITE, mmflags, sa->fd, sa->size);
	if (ret == MAP_FAILED) {
		ret = NULL;
		perror("mmap");
		goto restore_mempolicy;
	}

	if (alignment == 0 || ((uintptr_t) ret)%alignment == 0) {
		// we are lucky and got the right alignment
		goto success;
	}

	// the alignment didn't work out, munmap and try again
	munmap(ret, size);
	ret = NULL;

	// if new_addr is set, we can't fulfill the alignment, so just fail
	if (new_addr != NULL)
		goto restore_mempolicy;

	size += alignment;
	ret = mmap(NULL, size, PROT_READ | PROT_WRITE, mmflags, sa->fd, sa->size);
	if (ret == MAP_FAILED) {
		perror("mmap2");
		ret = NULL;
		goto restore_mempolicy;
	}

	n = (uintptr_t) ret;
	m = n + alignment - (n%alignment);
	munmap(ret, m-n);
	ret = (void *) m;

success:
	if (mbind(ret, size, mpol, nodemaskp, maxnode, MPOL_MF_MOVE) < 0) {
		munmap(ret, size);
		perror("mbind");
		ret = NULL;
		goto restore_mempolicy;
	}

  /* Add the extent to the array of extents */
  extent_arr_insert(sa->extents, ret, (char *)ret + size, NULL);

  /* Call the callback on this chunk if it's set */
  if(sicm_extent_alloc_callback) {
    (*sicm_extent_alloc_callback)(sa, ret, (char *)ret + size);
  }

	if (sa->fd) {
		sa->size += size;

		// only extend file; do not shrink
		// FIXME: how does that make sense, Jason???
		if (sa->size > lseek(sa->fd, 0, SEEK_END)) {
			ftruncate(sa->fd, sa->size);
			fsync(sa->fd);
		}
	}

restore_mempolicy:
	set_mempolicy(oldmode, oldnodemask->maskp, oldnodemask->size);

free_nodemasks:
	numa_free_nodemask(oldnodemask);
	pthread_mutex_unlock(sa->mutex);

	return ret;
}

static bool sa_dalloc(extent_hooks_t *h, void *addr, size_t size, bool committed, unsigned arena_ind) {
	sarena *sa;
	bool ret;

	ret = false;
	sa = container_of(h, sarena, hooks);
	pthread_mutex_lock(sa->mutex);
	extent_arr_delete(sa->extents, addr);

	if (munmap(addr, size) != 0) {
		fprintf(stderr, "munmap failed: %p %ld\n", addr, size);
		extent_arr_insert(sa->extents, addr, (char *)addr + size, NULL);
		ret = true;
	}
	sa->size -= size;

	if((!ret) && sicm_extent_dalloc_callback) {
		(*sicm_extent_dalloc_callback)(sa, addr, (char *)addr + size);
	}

	pthread_mutex_unlock(sa->mutex);
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
