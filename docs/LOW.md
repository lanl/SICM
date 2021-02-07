# The Low-Level Interface

| Function Name | Description |
|---------------|-------------|
| `sicm_init`  | Detects all memory devices on system, returns a list of them. |
| `sicm_fini`  | Frees up a device list and associated SICM data structures. |
| `sicm_find_device` | Return the first device that matches a given type and page size. |
| `sicm_device_alloc` | Allocates to a given device. |
| `sicm_device_free` | Frees memory on a device. |
| `sicm_can_place_exact` | Returns whether or not a device supports exact placement. |
| `sicm_device_alloc_exact` | Allocate memory on a device with an exact base address. |
| `sicm_numa_id` | Returns the NUMA ID that a device is on. |
| `sicm_device_page_size` | Returns the page size of a given device. |
| `sicm_device_eq` | Returns if two devices are equal or not. |
| `sicm_move`| Moves memory from one device to another. |
| `sicm_pin` | Pin the current process to a device's memory. |
| `sicm_capacity` | Returns the capacity of a given device. |
| `sicm_avail` | Returns the amount of memory available on a given device. |
| `sicm_model_distance` | Returns the distance of a given memory device. |
| `sicm_is_near` | Returns whether or not a given memory device is nearby the current NUMA node. |
| `sicm_latency` | Measures the latency of a memory device. |
| `sicm_bandwidth_linear2` | Measures a memory device's linear access bandwidth. |
| `sicm_bandwidth_random2` | Measures random access bandwidth of a memory device. |
| `sicm_bandwidth_linear3` | Measures the linear bandwidth of a memory device. |
| `sicm_bandwidth_random3` | Measures the random access bandwidth of a memory device. |

## Arena Allocator API

| Function Name | Description |
|---------------|-------------|
| `sicm_arenas_list` | List all arenas created in the arena allocator. |
| `sicm_arena_create` | Create a new arena on the given device. |
| `sicm_arena_destroy` | Frees up an arena, deleting all associated data structures. |
| `sicm_arena_set_default` | Sets an arena as the default for the current thread. |
| `sicm_arena_get_default` | Gets the default arena for the current thread. |
| `sicm_arena_get_device` | Gets the device for a given arena. |
| `sicm_arena_set_device` | Sets the memory device for a given arena. Moves all allocated memory already allocated to the arena. |
| `sicm_arena_size` | Gets the size of memory allocated to the given arena. |
| `sicm_arena_alloc` | Allocate to a given arena. |
| `sicm_arena_alloc_aligned` | Allocate aligned memory to a given arena. |
| `sicm_arena_realloc` | Resize allocated memory to a given arena. |
| `sicm_arena_lookup` | Returns which arena a given pointer belongs to. |
