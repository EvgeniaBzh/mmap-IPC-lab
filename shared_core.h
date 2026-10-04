/* shared_core.h - спільне ядро для mmap_bench і shm_bench:
 * регіон у спільній пам'яті з (а) ping-pong латентністю (spin або POSIX sem) і
 * (б) SPSC кільцевим буфером для throughput. Не залежить від способу створення регіону. */
#ifndef SHARED_CORE_H
#define SHARED_CORE_H
#include "common.h"
#include <stdatomic.h>
#include <semaphore.h>
#include <stddef.h>
#include <sys/mman.h>

#define RING_SIZE (1u << 22)
#define LAT_MAX   65536
#define MAGIC     0x49504331u

typedef struct {
    _Atomic uint32_t magic __attribute__((aligned(64)));
    _Atomic uint32_t ready;
    _Atomic uint32_t start;
    _Atomic uint32_t done;
    _Atomic uint32_t errors;
    _Atomic uint32_t turn __attribute__((aligned(64)));   /* 0 - хід батька, 1 - хід дитини */
    sem_t s_p2c __attribute__((aligned(64)));
    sem_t s_c2p __attribute__((aligned(64)));
    char data[LAT_MAX] __attribute__((aligned(64)));
    _Atomic uint64_t head __attribute__((aligned(64)));  /* пише продюсер */
    _Atomic uint64_t tail __attribute__((aligned(64)));  /* пише консюмер */
    char ring[RING_SIZE] __attribute__((aligned(64)));
} region_t;

#define HUGE_2M (2ul << 20)
static size_t region_bytes(void) { return (sizeof(region_t) + HUGE_2M - 1) & ~(HUGE_2M - 1); }

static void region_init(region_t *r) {
    memset(r, 0, sizeof *r);
    if (sem_init(&r->s_p2c, 1, 0) || sem_init(&r->s_c2p, 1, 0)) DIE("sem_init");
    atomic_store_explicit(&r->magic, MAGIC, memory_order_release);
}
static void wait_flag(_Atomic uint32_t *f, uint32_t v) {
    while (atomic_load_explicit(f, memory_order_acquire) != v) relax();
}
static void sem_wait_loop(sem_t *s) { while (sem_wait(s) < 0 && errno == EINTR) ; }

/* ---------------- latency ---------------- */
static void core_parent_lat(region_t *r, const char *name, size_t msg, size_t iters,
                            int use_sem, void (*hook)(void)) {
    if (msg < 1) msg = 1; if (msg > LAT_MAX) msg = LAT_MAX;
    uint64_t *s = malloc(iters * sizeof *s);
    wait_flag(&r->ready, 1);
    for (size_t i = 0; i < iters + WARMUP; i++) {
        uint64_t t0 = now_ns();
        memset(r->data, (int)(i & 0xff), msg);          /* "передача" даних */
        if (hook) hook();                                /* напр. msync() */
        if (use_sem) { sem_post(&r->s_p2c); sem_wait_loop(&r->s_c2p); }
        else {
            atomic_store_explicit(&r->turn, 1, memory_order_release);
            while (atomic_load_explicit(&r->turn, memory_order_acquire) != 0) relax();
        }
        uint64_t t1 = now_ns();
        if (r->data[0] != (char)((i + 1) & 0xff)) atomic_fetch_add(&r->errors, 1);
        if (i >= WARMUP) s[i - WARMUP] = (t1 - t0) / 2;
    }
    char full[160]; snprintf(full, sizeof full, "%s_%s", name, use_sem ? "sem" : "spin");
    print_lat(full, msg, s, iters);
    if (atomic_load(&r->errors)) fprintf(stderr, "DATA ERRORS: %u\n", atomic_load(&r->errors));
    free(s);
}
static void core_child_lat(region_t *r, size_t iters, int use_sem) {
    atomic_store_explicit(&r->ready, 1, memory_order_release);
    for (size_t i = 0; i < iters + WARMUP; i++) {
        if (use_sem) sem_wait_loop(&r->s_p2c);
        else while (atomic_load_explicit(&r->turn, memory_order_acquire) != 1) relax();
        r->data[0]++;                                    /* "обробка" */
        if (use_sem) sem_post(&r->s_c2p);
        else atomic_store_explicit(&r->turn, 0, memory_order_release);
    }
}

/* ---------------- throughput: SPSC ring ---------------- */
static void ring_put(region_t *r, const char *src, size_t n) {
    uint64_t h = atomic_load_explicit(&r->head, memory_order_relaxed);
    while (h + n - atomic_load_explicit(&r->tail, memory_order_acquire) > RING_SIZE) relax();
    size_t off = h & (RING_SIZE - 1), first = RING_SIZE - off; if (first > n) first = n;
    memcpy(r->ring + off, src, first);
    memcpy(r->ring, src + first, n - first);
    atomic_store_explicit(&r->head, h + n, memory_order_release);
}
static void ring_get(region_t *r, char *dst, size_t n) {
    uint64_t t = atomic_load_explicit(&r->tail, memory_order_relaxed);
    while (atomic_load_explicit(&r->head, memory_order_acquire) - t < n) relax();
    size_t off = t & (RING_SIZE - 1), first = RING_SIZE - off; if (first > n) first = n;
    memcpy(dst, r->ring + off, first);
    memcpy(dst + first, r->ring, n - first);
    atomic_store_explicit(&r->tail, t + n, memory_order_release);
}
static void core_parent_thr(region_t *r, const char *name, size_t block, size_t total_bytes) {
    if (block < 8) block = 8; if (block > RING_SIZE) block = RING_SIZE;
    size_t nblk = total_bytes / block;
    char *src = aligned_alloc(64, (block + 63) & ~63ul); memset(src, 0xAB, block);
    wait_flag(&r->ready, 1);
    uint64_t t0 = now_ns();
    atomic_store_explicit(&r->start, 1, memory_order_release);
    for (size_t b = 0; b < nblk; b++) { uint64_t seq = b; memcpy(src, &seq, 8); ring_put(r, src, block); }
    wait_flag(&r->done, 1);
    uint64_t t1 = now_ns();
    print_thr(name, block, nblk * block, t1 - t0);
    if (atomic_load(&r->errors)) fprintf(stderr, "DATA ERRORS: %u\n", atomic_load(&r->errors));
    free(src);
}
static void core_child_thr(region_t *r, size_t block, size_t total_bytes) {
    if (block < 8) block = 8; if (block > RING_SIZE) block = RING_SIZE;
    size_t nblk = total_bytes / block;
    char *dst = aligned_alloc(64, (block + 63) & ~63ul);
    atomic_store_explicit(&r->ready, 1, memory_order_release);
    wait_flag(&r->start, 1);
    for (size_t b = 0; b < nblk; b++) {
        ring_get(r, dst, block);
        uint64_t seq; memcpy(&seq, dst, 8);
        if (seq != b) atomic_fetch_add(&r->errors, 1);
    }
    atomic_store_explicit(&r->done, 1, memory_order_release);
    free(dst);
}
#endif
