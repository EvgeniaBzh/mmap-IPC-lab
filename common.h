#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sched.h>

static inline uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static void pin_cpu(int cpu) {
    cpu_set_t s; CPU_ZERO(&s); CPU_SET(cpu, &s);
    if (sched_setaffinity(0, sizeof s, &s) != 0) perror("affinity");
}

/* read/write можуть повернути менше, ніж просили: треба цикл */
static void read_full(int fd, void *buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, (char*)buf + got, n - got);
        if (r < 0) { if (errno == EINTR) continue; perror("read"); exit(1); }
        if (r == 0) { fprintf(stderr, "EOF\n"); exit(1); }
        got += r;
    }
}
static void write_full(int fd, const void *buf, size_t n) {
    size_t put = 0;
    while (put < n) {
        ssize_t w = write(fd, (const char*)buf + put, n - put);
        if (w < 0) { if (errno == EINTR) continue; perror("write"); exit(1); }
        put += w;
    }
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t*)a, y = *(const uint64_t*)b;
    return (x > y) - (x < y);
}

/* samples: one-way latency в нс */
static void print_lat(const char *name, size_t msg, uint64_t *s, size_t n) {
    qsort(s, n, sizeof *s, cmp_u64);
    double sum = 0; for (size_t i = 0; i < n; i++) sum += s[i];
    printf("%s,lat,%zu,%.1f,%lu,%lu,%lu\n", name, msg,
           sum / n, s[n/2], s[(size_t)(n*0.99)], s[n-1]);
    /* формат: name,mode,msg_size,mean_ns,median_ns,p99_ns,max_ns */
}

static void print_thr(const char *name, size_t block, size_t total, uint64_t ns) {
    double mbps = (double)total / (1024.0*1024.0) / (ns / 1e9);
    printf("%s,thr,%zu,%.1f\n", name, block, mbps);   /* MB/s */
}