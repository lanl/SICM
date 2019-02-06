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

	devs = sicm_init();
	d1 = &devs.devices[0];
	pgsz = sicm_device_page_size(d1);
	for(i = 1; i < devs.count; i++) {
		d2 = &devs.devices[i];
		if (sicm_device_page_size(d2) == pgsz)
			break;
	}

	s1 = sicm_arena_create(0, d1);
	if (s1 == NULL) {
		fprintf(stderr, "sarena_create failed\n");
		return -1;
	}

	s2 = sicm_arena_create(0, d2);
	if (s2 == NULL) {
		fprintf(stderr, "sarena_create failed\n");
		return -1;
	}

	if ((buf1 = sicm_arena_alloc(s2, 8*1024*1024*1024LL)) == NULL) {
		fprintf(stderr, "huge alloc failed\n");
		return -1;
	}

	if (sicm_arena_set_device(s2, d2) < 0) {
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
	if (sicm_arena_set_device(s1, d2) < 0) {
		fprintf(stderr, "move failed gracefully\n");
		return 0;
	}

	return 0;
}
