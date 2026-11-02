#ifndef UTIL_H
#define UTIL_H

#include "cosas.h"

static void nombre_casilla(int id, char* buf, size_t n) {
    if (id == TRONO_TOP) snprintf(buf, n, "TRONO_TOP");
    else if (id == TRONO_LEFT) snprintf(buf, n, "TRONO_LEFT");
    else if (id == TRONO_RIGHT) snprintf(buf, n, "TRONO_RIGHT");
    else if (id >= 0 && id < 25) snprintf(buf, n, "(%d,%d)", id % 5, id / 5);
    else snprintf(buf, n, "?%d", id);
}

static void nombre_ficha(int id, char* buf, size_t n) {
    if (id == 0) snprintf(buf, n, "Puma");
    else snprintf(buf, n, "Perro %d", id - 1);
}

static void ms_a_abs(struct timespec* ts, int ms) {
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += ms / 1000;
    ts->tv_nsec += (long)(ms % 1000) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000L;
    }
}

static void sleep_ms(int ms) {
    struct timespec r = {ms / 1000, (long)(ms % 1000) * 1000000L};
    while (nanosleep(&r, &r) == -1 && errno == EINTR) {
    }
}

#endif
