#include <stdio.h>
#include <sicm_low.h>

sicm_device_list devs;

int main() {
	int i;
	sicm_arena s;
	char *buf1, *buf2;
	sicm_device_list ds;

	devs = sicm_init();
	for(i = 0; i < devs.count; i++) {
		printf("%d %d\n", i, sicm_numa_id(devs.devices[i]));
	}

	ds.count = 1;
	ds.devices = &devs.devices[0];
	s = sicm_arena_create(0, 0, &ds);
	if (s == NULL) {
		fprintf(stderr, "sicm_arena_create failed\n");
		return -1;
	}

	buf1 = sicm_arena_alloc(s, 1024);
	buf2 = sicm_arena_alloc(s, 2048*1024);
	sicm_free(buf1);
	sicm_free(buf2);

	sicm_arena_destroy(s);
	sicm_fini();

	return 0;
}
