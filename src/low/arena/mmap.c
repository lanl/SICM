#include <numa.h>
#include <numaif.h>
#include <unistd.h>
#include <sys/mman.h>

#include "sicm_impl.h"

static void *sa_alloc(extent_hooks_t *h, void *new_addr, size_t size, size_t alignment, bool *zero, bool *commit, unsigned arena_ind) {
	int mpol;
	unsigned long *nodemaskp, maxnode;
	sarena *sa;
	uintptr_t n, m;
	int oldmode, mmflags;
	void *ret;
	struct bitmask *oldnodemask;

	*commit = 0;
	*zero = 0;
	ret = NULL;
	sa = container_of(h, sarena, hooks);

	// TODO: figure out a way to prevent taking the mutex twice (sa_range_add also takes it)...
	pthread_mutex_lock(sa->mutex);
	if (sa->maxsize > 0 && sa->size + size > sa->maxsize) {
		return NULL;
	}

	oldnodemask = numa_allocate_nodemask();
	get_mempolicy(&oldmode, oldnodemask->maskp, oldnodemask->size + 1, NULL, 0);
	switch (sa->flags & SICM_ALLOC_MASK) {
	case SICM_ALLOC_STRICT:
		mpol = MPOL_BIND;
		nodemaskp = sa->nodemask->maskp;
		maxnode = sa->nodemask->size + 1;
		break;

	case SICM_ALLOC_RELAXED:
		// TODO: this will work only for single device, fix it
		mpol = MPOL_PREFERRED;
		nodemaskp = sa->nodemask->maskp;
		maxnode = sa->nodemask->size + 1;
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
		mmflags = MAP_ANONYMOUS|MAP_PRIVATE|MAP_POPULATE;
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

success:
	if (mbind(ret, size, mpol, nodemaskp, maxnode, MPOL_MF_MOVE) < 0) {
		munmap(ret, size);
		perror("mbind");
		ret = NULL;
		goto restore_mempolicy;
	}

	if (!(alignment == 0 || ((uintptr_t) ret)%alignment == 0)) {
		n = (uintptr_t) ret;
		m = n + alignment - (n%alignment);
		ret = (void *) m;
		size -= alignment;
	}

	/* Add the extent to the array of extents */
	extent_arr_insert(sa->extents, ret, (char *)ret + size, NULL);

	/* Call the callback on this chunk if it's set */
	if(sicm_extent_alloc_callback) {
		(*sicm_extent_alloc_callback)(ret, (char *)ret + size);
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
	set_mempolicy(oldmode, oldnodemask->maskp, oldnodemask->size + 1);

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

extent_hooks_t sicm_arena_mmap_hooks = {
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
