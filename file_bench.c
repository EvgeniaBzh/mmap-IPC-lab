/* file I/O vs mmap для файлів (throughput)
 *   ./file_bench <variant> <block> [total_MiB=512]
 * variants: write, write_fsync, write_odirect,
 *           read, read_odirect, read_rand,
 *           mmap_read, mmap_seq, mmap_rand, mmap_write, mmap_write_msync
 * для холодного кешу: sync; echo 3 | sudo tee /proc/sys/vm/drop_caches  (або *_odirect). */
#include "common.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

static volatile uint64_t g_sink;
static uint64_t sum64(const void *p, size_t n) {
    const uint64_t *w = p; uint64_t s = 0;
    for (size_t i = 0; i < n / 8; i++) s += w[i];
    return s;
}
static size_t gcd(size_t a, size_t b) { while (b) { size_t t = a % b; a = b; b = t; } return a; }

static void ensure_file(const char *path, size_t total) {
    struct stat st;
    if (stat(path, &st) == 0 && (size_t)st.st_size == total) return;
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) DIE("open");
    char *b = malloc(1 << 20); memset(b, 7, 1 << 20);
    for (size_t o = 0; o < total; o += 1 << 20) write_full(fd, b, 1 << 20);
    fsync(fd); close(fd); free(b);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <variant> <block> [MiB]\n", argv[0]); return 2; }
    const char *v = argv[1];
    size_t block = (size_t)atoll(argv[2]);
    size_t total = (argc > 3 ? (size_t)atoll(argv[3]) : 512) << 20;
    if (block < 4096) block = 4096;
    total = total / block * block;
    size_t nblk = total / block;
    const char *path = getenv("FILE_PATH") ? getenv("FILE_PATH") : "./ipc_file.dat";
    char name[96]; snprintf(name, sizeof name, "file_%s", v);
    char *buf; if (posix_memalign((void **)&buf, 4096, block)) DIE("memalign");
    memset(buf, 0x5A, block);
    int is_write = !strncmp(v, "write", 5), is_mmap = !strncmp(v, "mmap", 4);
    uint64_t t0, t1;

    if (is_write) {
        int fl = O_WRONLY | O_CREAT | O_TRUNC;
        if (!strcmp(v, "write_odirect")) fl |= O_DIRECT;
        int fd = open(path, fl, 0600);
        if (fd < 0) DIE("open (O_DIRECT не підтримується на tmpfs - задай FILE_PATH на диск)");
        t0 = now_ns();
        for (size_t b = 0; b < nblk; b++) { memcpy(buf, &b, 8); write_full(fd, buf, block); }
        if (!strcmp(v, "write_fsync")) fsync(fd);
        close(fd);
        t1 = now_ns();
    } else if (!strcmp(v, "mmap_write") || !strcmp(v, "mmap_write_msync")) {
        int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);
        if (fd < 0) DIE("open");
        if (ftruncate(fd, (off_t)total)) DIE("ftruncate");
        t0 = now_ns();
        char *m = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (m == MAP_FAILED) DIE("mmap");
        for (size_t b = 0; b < nblk; b++) { memcpy(buf, &b, 8); memcpy(m + b * block, buf, block); }
        if (!strcmp(v, "mmap_write_msync")) msync(m, total, MS_SYNC);
        munmap(m, total); close(fd);
        t1 = now_ns();
    } else {
        ensure_file(path, total);
        size_t stride = 1;
        if (!strcmp(v, "read_rand") || !strcmp(v, "mmap_rand")) {   /* псевдовипадковий порядок блоків */
            stride = (size_t)(2654435761ull % nblk) | 1; while (gcd(stride, nblk) != 1) stride += 2;
        }
        uint64_t acc = 0;
        if (is_mmap) {
            int fd = open(path, O_RDONLY); if (fd < 0) DIE("open");
            t0 = now_ns();
            char *m = mmap(NULL, total, PROT_READ, MAP_SHARED, fd, 0);
            if (m == MAP_FAILED) DIE("mmap");
            if (!strcmp(v, "mmap_seq"))  madvise(m, total, MADV_SEQUENTIAL);
            if (!strcmp(v, "mmap_rand")) madvise(m, total, MADV_RANDOM);
            for (size_t i = 0; i < nblk; i++) acc += sum64(m + ((i * stride) % nblk) * block, block);
            munmap(m, total); close(fd);
            t1 = now_ns();
        } else {
            int fl = O_RDONLY | (!strcmp(v, "read_odirect") ? O_DIRECT : 0);
            int fd = open(path, fl); if (fd < 0) DIE("open");
            t0 = now_ns();
            for (size_t i = 0; i < nblk; i++) {
                ssize_t r = pread(fd, buf, block, (off_t)(((i * stride) % nblk) * block));
                if (r != (ssize_t)block) { fprintf(stderr, "short read\n"); return 1; }
                acc += sum64(buf, block);
            }
            close(fd);
            t1 = now_ns();
        }
        g_sink = acc;
    }
    print_thr(name, block, total, t1 - t0);
    if (is_write || !strcmp(v, "mmap_write")) unlink(path);
    return 0;
}
