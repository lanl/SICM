#include <stdio.h>
#include <string.h>
#include <sicm_low.h>

int main() {
	sicm_device_list devs = sicm_init();

	for(int i = 0; i < devs.count; i++) {
		sicm_device *dev = devs.devices[i];
		printf("%d %d %s %d\n", i, sicm_numa_id(dev), sicm_device_tag_str(dev->tag), dev->page_size);
	}

	for(int i = 0; i < devs.count; i++) {
		sicm_device *dev = devs.devices[i];
		sicm_device_list ds = {
			.count = 1,
			.devices = &dev,
		};

		sicm_arena s = sicm_arena_create(0, 0, &ds);
		if (s == NULL) {
			fprintf(stderr, "sicm_arena_create failed for device %d: %d %d %s (%d)\n",
					i, sicm_numa_id(dev), dev->page_size,
					sicm_device_tag_str(dev->tag), dev->tag);
			return -1;
		}

		char *buf1 = sicm_arena_alloc(s, 1024);
		char *buf2 = sicm_arena_alloc(s, 2048*1024);
		memset(buf1, 0, 1024);
		memset(buf2, 0, 2048*1024);
		sicm_free(buf2);
		sicm_free(buf1);

		sicm_arena_destroy(s);
	}

	sicm_fini();
	return 0;
}
