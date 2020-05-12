#include <stdio.h>
#include <sicm_low.h>

#define N 100000

sicm_device_list devs;

int main() {
	sicm_arena s1, s2;
	char *buf1[N], *buf2;
	sicm_device_list ds;

	devs = sicm_init();
	ds.count = 1;
	ds.devices = &devs.devices[0];
	s1 = sicm_arena_create(0, 0, &ds);
	if (s1 == NULL) {
		fprintf(stderr, "sarena_create failed\n");
		return -1;
	}

	ds.devices = &devs.devices[devs.count - 1];
	s2 = sicm_arena_create(0, 0, &ds);
	if (s2 == NULL) {
		fprintf(stderr, "sarena_create failed\n");
		return -1;
	}

	for(int i = 0; i < N; i++) {
		buf1[i] = sicm_arena_alloc(s2, 200);
	}

	buf2 = sicm_arena_alloc(s2, 4092);

	for(int i = 0; i < N; i++) {
		sicm_free(buf1[i]);
	}

	sicm_free(buf2);

	sicm_arena_destroy(s1);
	sicm_arena_destroy(s2);
	sicm_fini();

	return 0;
}
