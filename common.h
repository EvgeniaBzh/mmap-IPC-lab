#ifndef COMMON_H
#define COMMON_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sched.h>
#include <sys/resource.h>

#define DIE(msg) do { perror(msg); exit(1); } while (0)
#define WARMUP 10000

#if defined(__x86_64__) || defined(__i386__)
#define CPU_RELAX() __builtin_ia32_pause()
#elif defined(__aarch64__)
#define CPU_RELAX() __asm__ volatile("yield")
#else
#define CPU_RELAX() ((void)0)
#endif

// якщо обидва процеси на одному ядрі, спін без sched_yield зависне надовго
static int g_yield = 0;
static inline void relax(void) { if (g_yield) sched_yield(); else CPU_RELAX(); }

static inline uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* role 0 = батько (producer/ініціатор), role 1 = дитина.
 * керування: IPC_SAME_CORE=1 (обидва на одному ядрі), IPC_NO_PIN=1 (без pin),
 * IPC_CPU_A / IPC_CPU_B (конкретні ядра). */
static void pin_role(int role) {
    cpu_set_t m; CPU_ZERO(&m);
    sched_getaffinity(0, sizeof m, &m);
    int list[1024], n = 0;
    for (int i = 0; i < CPU_SETSIZE && n < 1024; i++) if (CPU_ISSET(i, &m)) list[n++] = i;
    if (n == 0) return;
    int a = list[0], b = n > 1 ? list[1] : list[0];
    if (getenv("IPC_CPU_A")) a = atoi(getenv("IPC_CPU_A"));
    if (getenv("IPC_CPU_B")) b = atoi(getenv("IPC_CPU_B"));
    const char *same = getenv("IPC_SAME_CORE");
    if (same && atoi(same)) b = a;
    g_yield = (a == b) || n < 2;
    if (getenv("IPC_NO_PIN")) return;
    int cpu = role == 0 ? a : b;
    cpu_set_t s; CPU_ZERO(&s); CPU_SET(cpu, &s);
    if (sched_setaffinity(0, sizeof s, &s) != 0) perror("sched_setaffinity");
}

static void read_full(int fd, void *buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, (char *)buf + got, n - got);
        if (r < 0) { if (errno == EINTR) continue; DIE("read"); }
        if (r == 0) { fprintf(stderr, "unexpected EOF\n"); exit(1); }
        got += (size_t)r;
    }
}
static void write_full(int fd, const void *buf, size_t n) {
    size_t put = 0;
    while (put < n) {
        ssize_t w = write(fd, (const char *)buf + put, n - put);
        if (w < 0) { if (errno == EINTR) continue; DIE("write"); }
        put += (size_t)w;
    }
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

// samples = one-way latency у нс. формат csv: name,lat,msg,mean,median,p99,max
static void print_lat(const char *name, size_t msg, uint64_t *s, size_t n) {
    qsort(s, n, sizeof *s, cmp_u64);
    double sum = 0; for (size_t i = 0; i < n; i++) sum += (double)s[i];
    printf("%s,lat,%zu,%.1f,%llu,%llu,%llu\n", name, msg, sum / (double)n,
           (unsigned long long)s[n / 2], (unsigned long long)s[(size_t)((double)n * 0.99)],
           (unsigned long long)s[n - 1]);
}
// формат csv: name,thr,block,MB/s
static void print_thr(const char *name, size_t block, size_t total, uint64_t ns) {
    double mbps = (double)total / (1024.0 * 1024.0) / ((double)ns / 1e9);
    printf("%s,thr,%zu,%.1f\n", name, block, mbps);
}

// cpu/context switches (у stderr, щоб не ламати CSV)
static void print_rusage(const char *name) {
    struct rusage a, c;
    getrusage(RUSAGE_SELF, &a); getrusage(RUSAGE_CHILDREN, &c);
    fprintf(stderr, "[rusage] %s: user=%.3fs sys=%.3fs (children user=%.3fs sys=%.3fs) "
            "vcsw=%ld ivcsw=%ld minflt=%ld\n", name,
            a.ru_utime.tv_sec + a.ru_utime.tv_usec / 1e6, a.ru_stime.tv_sec + a.ru_stime.tv_usec / 1e6,
            c.ru_utime.tv_sec + c.ru_utime.tv_usec / 1e6, c.ru_stime.tv_sec + c.ru_stime.tv_usec / 1e6,
            a.ru_nvcsw + c.ru_nvcsw, a.ru_nivcsw + c.ru_nivcsw, a.ru_minflt + c.ru_minflt);
}
#endif
