#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "sicm_low.h"

#define SIZE 1024 * 1024 * 8
#define FILENAME "FILE"

#ifndef SICM_DEVICE_TYPE
#define SICM_DEVICE_TYPE SICM_DRAM
#endif

int main() {
    srand(time(NULL));

    // find a device
    sicm_device_list devs = sicm_init();
    sicm_device *dev = NULL;
    for(unsigned int i = 0; i < devs.count; i++) {
        if (devs.devices[i].tag == SICM_DEVICE_TYPE) {
            dev = &devs.devices[i];
            break;
        }
    }

    if (!dev) {
        printf("Error: No device of type %s found\n", sicm_device_tag_str(SICM_DEVICE_TYPE));
        return 1;
    }

    // generate random string
    size_t len = rand() % SIZE ;
    char *orig = malloc(len);
    for(size_t i = 0; i < len; i += sizeof(int)) {
        int r = rand();
        memcpy(&orig[i], &r, sizeof(int));
    }

    printf("Generated %zu bytes of data\n", len);

    remove(FILENAME);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork\n");
        return 1;
    }

    // each process open its own copy of the file, instead of duplicating the file descriptor

    // open file for arbitrary data
    int fd = open(FILENAME, O_CREAT | O_APPEND | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    ftruncate(fd, SIZE);
    lseek(fd, 0, SEEK_SET);
    fsync(fd);

    sicm_arena arena = sicm_arena_create_mmapped(0, dev, fd, 0, -1, 0);
    char *ptr = sicm_arena_alloc(arena, len);

    // child writes to the file
    if (pid == 0) {
        memcpy(ptr, orig, len);
    }
    // parent waits for the child and compares the file to the data in memory
    else {
        wait(NULL);

        if (memcmp(ptr, orig, len)) {
            printf("Did not get the same data back\n");
            printf("%zu bytes\n", len);
            for(size_t i = 0; i < len; i++) {
                if (orig[i] != ptr[i]) {
                    printf("%zu %02x %02x\n", i, orig[i] & 0xff, ptr[i] & 0xff);
                }
            }
        }
        else {
            printf("No difference\n");
        }
    }

    // cleanup
    sicm_free(ptr);
    close(fd);
    free(orig);
    remove(FILENAME);

    return 0;
}
