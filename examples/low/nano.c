#include "nano.h"

double nano(struct timespec *start, struct timespec *end) {
    double s = start->tv_sec;
    s *= 1e9;
    s += start->tv_nsec;

    double e = end->tv_sec;
    e *= 1e9;
    e += end->tv_nsec;

    return e - s;
}
