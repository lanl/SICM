#include <stdio.h>
#include <sicm_low.h>
#include <unistd.h>

#define N 100000

sicm_device_list devs;

int main() {
	int i;
	sicm_arena s1, s2;
	sicm_device *d1, *d2;
	size_t pgsz;
	char *buf1, *buf2;
	sicm_device_list ds;

	devs = sicm_init();
	d1 = devs.devices[0];
	pgsz = sicm_device_page_size(d1);
	for(i = 1; i < devs.count; i++) {
		d2 = devs.devices[i];
		if (sicm_device_page_size(d2) == pgsz)
			break;
	}

	ds.count = 1;
	ds.devices = &d1;
	s1 = sicm_arena_create(0, 0, &ds);
	if (s1 == NULL) {
		fprintf(stderr, "sarena_create failed\n");
		return -1;
	}

	ds.devices = &d2;
	s2 = sicm_arena_create(0, 0, &ds);
	if (s2 == NULL) {
		fprintf(stderr, "sarena_create failed\n");
		return -1;
	}

	if ((buf1 = sicm_arena_alloc(s2, 8*1024*1024*1024LL)) == NULL) {
		fprintf(stderr, "huge alloc failed\n");
		return -1;
	}

	if (sicm_arena_set_devices(s2, &ds) < 0) {
		fprintf(stderr, "move failed\n");
		return -1;
	}

	buf2 = sicm_arena_alloc(s1, 4*1024*1024*1024LL);
	if (buf2 == NULL) {
		fprintf(stderr, "second huge alloc failed\n");
		return -1;
	}

	printf("so far so good buf1 %p buf2 %p\n", buf1, buf2);
//	sleep(60);

	printf("moving second huge alloc (s1) to device 2...\n");
	if (sicm_arena_set_devices(s1, &ds) < 0) {
		fprintf(stderr, "move failed gracefully\n");
		return 0;
	}

	sicm_arena_destroy(s1);
	sicm_arena_destroy(s2);
	sicm_fini();

	return 0;
}
