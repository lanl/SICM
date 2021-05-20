#ifndef __PROC_OBJECT_MAP_H__
#define __PROC_OBJECT_MAP_H__

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

struct proc_object_map_record_t {
    int n_resident_pages;
    int coop_buff_n_bytes;
};

struct proc_object_map_t {
    int          fd;
    unsigned int page_size;
};

int objmap_open(struct proc_object_map_t *objmap);
int objmap_close(struct proc_object_map_t *objmap);
int objmap_add_range(struct proc_object_map_t *objmap, void *start, void *end);
int objmap_del_range(struct proc_object_map_t *objmap, void *start);
int objmap_entry_read_record(const char *entry_path, struct proc_object_map_record_t *record_p);
int objmap_entry_read_record_coop_buff(const char *entry_path, void *buff, int n_coop_buff_bytes);

#endif

#ifdef PROC_OBJECT_MAP_IMPL

/*
 * The following definitions MUST match the ones in
 * the kernel file (fs/proc/object_map.c).
 */
#define OBJECT_MAP_CONTROLLER_MAGIC (0x0BC01234)

#define OBJECT_MAP_CONTROLLER_CMD_ADD (1)
#define OBJECT_MAP_CONTROLLER_CMD_DEL (2)

__attribute__((packed))
struct object_map_controller_info {
    unsigned int       _magic;
    int                cmd;
    int                node;
    unsigned long long range_start;
    unsigned long long range_end;
};

static char *my_itoa(char *p, unsigned x) {
    p += 3*sizeof(int);
    *--p = 0;
    do {
        *--p = '0' + x % 10;
        x /= 10;
    } while (x);
    return p;
}

static int open_proc_fs_entry(pid_t pid) {
    char  path[4096];
    char  pid_str_buff[32];
    char *pid_str;
    int   fd;

    /*
     * Can't use sprintf because it might call malloc().
     */
/*     sprintf(path, "/proc/%d/object_map/controller", pid); */

    path[0] = 0;
    strcat(path, "/proc/");
    pid_str = my_itoa(pid_str_buff, pid);
    strcat(path, pid_str);
    strcat(path, "/object_map/controller");

    errno = 0;

    fd = open(path, O_WRONLY);

    if (fd == -1) {
        fd = -errno;
    }

    errno = 0;

    return fd;
}

static int write_object_map_controller_info(int fd, struct object_map_controller_info *info) {
    int status;

    errno  = 0;
    status = write(fd, info, sizeof(*info));

    if (status == -1) {
        fprintf(stderr, "Failed to write %zu bytes to fd %d, info %p\n", sizeof(*info), fd, info);
        status = -errno;
    }

    errno = 0;

    return status;
}

int objmap_open(struct proc_object_map_t *objmap) {
    pid_t pid;
    int   status;

    pid    = getpid();
    status = open_proc_fs_entry(pid);
    if (status >= 0) {
        objmap->fd        = status;
        objmap->page_size = sysconf(_SC_PAGE_SIZE);
    }

    return status;
}

int objmap_close(struct proc_object_map_t *objmap) {
    int status;

    status = close(objmap->fd);
    memset(objmap, 0, sizeof(*objmap));

    return status;
}

int objmap_add_range(struct proc_object_map_t *objmap, void *start, void *end) {
    struct object_map_controller_info info;
    int                               status;

    status = 0;

    if (end <= start) {
        /*  The end is before the start */
        status = -EINVAL;
        goto out;
    }

    if (((unsigned long long)start) & (objmap->page_size - 1)
    ||  ((unsigned long long)end)   & (objmap->page_size - 1)) {
        /* The address range isn't page-aligned */
        status = -EINVAL;
        goto out;
    }

    info._magic      = OBJECT_MAP_CONTROLLER_MAGIC;
    info.cmd         = OBJECT_MAP_CONTROLLER_CMD_ADD;
    info.range_start = (unsigned long long)start;
    info.range_end   = (unsigned long long)end;

    status = write_object_map_controller_info(objmap->fd, &info);

out:
    return status;
}

int objmap_del_range(struct proc_object_map_t *objmap, void *start) {
    struct object_map_controller_info info;
    int                               status;

    status = 0;

    if (((unsigned long long)start) & (objmap->page_size - 1)) {
        status = -EINVAL;
        goto out;
    }

    info._magic      = OBJECT_MAP_CONTROLLER_MAGIC;
    info.cmd         = OBJECT_MAP_CONTROLLER_CMD_DEL;
    info.range_start = (unsigned long long)start;
    info.range_end   = (unsigned long long)NULL;

    status = write_object_map_controller_info(objmap->fd, &info);

out:
    return status;
}

int objmap_entry_read_record(const char *entry_path, struct proc_object_map_record_t *record_p) {
    int status;
    int fd;
    int n_read;

#define TOTAL_BYTES (sizeof(struct proc_object_map_record_t))

    status = 0;

    if ((fd = open(entry_path, O_RDONLY)) < 0) {
        status = errno;
        errno  = 0;
        goto out;
    }

    n_read = 0;
    while ((n_read = read(fd, record_p + n_read, TOTAL_BYTES - n_read)) > 0) {}

    if (n_read < 0) {
        status = errno;
        errno  = 0;
        goto out_close;
    }

out_close:
    close(fd);
out:
    return status;

#undef TOTAL_BYTES
}

int objmap_entry_read_record_coop_buff(const char *entry_path, void *buff, int n_coop_buff_bytes) {
    int status;
    int fd;
    int n_read;

    status = 0;

    if ((fd = open(entry_path, O_RDONLY)) < 0) {
        status = errno;
        errno  = 0;
        goto out;
    }

    status = lseek(fd, sizeof(struct proc_object_map_record_t), SEEK_SET);
    if (status) {
        status = errno;
        errno  = 0;
        goto out_close;
    }

    n_read = 0;
    while ((n_read = read(fd, buff + n_read, n_coop_buff_bytes - n_read)) > 0) {}

    if (n_read < 0) {
        status = errno;
        errno  = 0;
        goto out_close;
    }

out_close:
    close(fd);
out:
    return status;
}

#endif
