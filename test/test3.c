#include <stdio.h>
#include <sicm_low.h>

#define N 100000

sicm_device_list devs;

int main() {
	int i;
	sicm_arena s1, s2;
	sicm_device *d1, *d2;
	size_t pgsz;
	char *buf1[N], *buf2;

	devs = sicm_init();
	d1 = &devs.devices[0];
	pgsz = sicm_device_page_size(d1);
	for(i = 1; i < devs.count; i++) {
		d2 = &devs.devices[i];
		if (sicm_device_page_size(d2) == pgsz)
			break;
	}

	s1 = sicm_arena_create(1000, d1);
	if (s1 == NULL) {
		fprintf(stderr, "sarena_create failed\n");
		return -1;
	}

	s2 = sicm_arena_create(1000, d2);
	if (s2 == NULL) {
		fprintf(stderr, "sarena_create failed\n");
		return -1;
	}

	for(int i = 0; i < N; i++) {
		buf1[i] = sicm_arena_alloc(s1, 200);
	}

	if (sicm_arena_set_device(s1, d1) < 0) {
		fprintf(stderr, "can't change memory policy\n");
		return -1;
	}

	buf2 = sicm_arena_alloc(s2, 4092);

	for(int i = 0; i < N; i++) {
		sicm_free(buf1[i]);
	}

	sicm_free(buf2);
	return 0;
}
