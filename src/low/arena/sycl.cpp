#include <CL/sycl.hpp>

#include "sicm_impl.h"

static void *sa_alloc(extent_hooks_t *h, void *new_addr, size_t size, size_t alignment, bool *zero, bool *commit, unsigned arena_ind) {
	sarena *sa;
    sicm_device *device;
	uintptr_t n, m;
	void *ret = NULL;
	struct bitmask *oldnodemask;

	if (new_addr) {
		return NULL;
	}

	*commit = 0;
	*zero = 0;
	ret = NULL;
	sa = container_of(h, sarena, hooks);
    device = sa->devs.devices[0];

	// TODO: figure out a way to prevent taking the mutex twice (sa_range_add also takes it)...
	pthread_mutex_lock(sa->mutex);
	if (sa->maxsize > 0 && sa->size + size > sa->maxsize) {
		goto done;
	}

	// arenas are created only on one device
    ret = sycl::malloc_device(size,
                             * (sycl::device *) device->data.sycl.device,
                             * (sycl::context *) device->data.sycl.context);
    if (!ret) {
		perror("sycl::malloc_device");
		goto done;
	}

	if (alignment == 0 || ((uintptr_t) ret)%alignment == 0) {
		// we are lucky and got the right alignment
		goto success;
	}

	// the alignment didn't work out, munmap and try again
    sycl::free(ret, * (sycl::context *) device->data.sycl.context);
	ret = NULL;

	size += alignment;
    ret = sycl::malloc_device(size,
                             * (sycl::device *) device->data.sycl.device,
                             * (sycl::context *) device->data.sycl.context);
    if (!ret) {
		perror("sycl::malloc_device 2");
		goto done;
	}

success:
	if (!(alignment == 0 || ((uintptr_t) ret)%alignment == 0)) {
		n = (uintptr_t) ret;
		m = n + alignment - (n%alignment);
		ret = (void *) m;
		size -= alignment;
	}

	/* Add the extent to the array of extents */
	extent_arr_insert(sa->extents, ret, (char *)ret + size, NULL);

done:
	pthread_mutex_unlock(sa->mutex);

	return ret;
}

static bool sa_dalloc(extent_hooks_t *h, void *addr, size_t size, bool committed, unsigned arena_ind) {
	sarena *sa;
    sicm_device *device;
	bool ret;

	ret = false;
	sa = container_of(h, sarena, hooks);
    device = sa->devs.devices[0];
	pthread_mutex_lock(sa->mutex);
	extent_arr_delete(sa->extents, addr);

    sycl::free(addr, * (sycl::context *) device->data.sycl.context);

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

extent_hooks_t sicm_arena_sycl_hooks = {
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
