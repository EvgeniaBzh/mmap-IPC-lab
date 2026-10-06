/* posix shared memory (shm_open + mmap).
 *   ./shm_bench lat <msg> <iters> [spin|sem] [both|a|b]
 *   ./shm_bench thr <block> <total_MiB> [-] [both|a|b]
 * both = fork (за замовчуванням); a/b = два неповязаних процеси: запусти a у одному
 * терміналі й b з тими ж параметрами у другому (a створює об'єкт, b підключається). */
#include "common.h"
#include "shared_core.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define SHM_NAME "/ipc_lab_shm"

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s <lat|thr> <size> <n> [spin|sem] [both|a|b]\n", argv[0]); return 2; }
    int lat = !strcmp(argv[1], "lat");
    size_t sz = (size_t)atoll(argv[2]), n = (size_t)atoll(argv[3]);
    int use_sem = argc > 4 && !strcmp(argv[4], "sem");
    const char *role = argc > 5 ? argv[5] : "both";
    size_t len = region_bytes();
    int is_b = !strcmp(role, "b");

    int fd;
    if (is_b) { do { fd = shm_open(SHM_NAME, O_RDWR, 0600); if (fd < 0) usleep(10000); } while (fd < 0); }
    else      { shm_unlink(SHM_NAME); fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0600); }
    if (fd < 0) DIE("shm_open");
    if (!is_b && ftruncate(fd, (off_t)len)) DIE("ftruncate");
    if (is_b) { struct stat st; do { fstat(fd, &st); if ((size_t)st.st_size < len) usleep(10000); } while ((size_t)st.st_size < len); }
    region_t *r = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (r == MAP_FAILED) DIE("mmap");
    close(fd);
    if (is_b) while (atomic_load_explicit(&r->magic, memory_order_acquire) != MAGIC) usleep(1000);
    else region_init(r);

    if (!strcmp(role, "both")) {
        pid_t pid = fork();
        if (pid == 0) {
            pin_role(1);
            if (lat) core_child_lat(r, n, use_sem); else core_child_thr(r, sz, n << 20);
            _exit(0);
        }
        pin_role(0);
        if (lat) core_parent_lat(r, "shm", sz, n, use_sem, NULL); else core_parent_thr(r, "shm", sz, n << 20);
        waitpid(pid, NULL, 0);
    } else if (is_b) {
        pin_role(1);
        if (lat) core_child_lat(r, n, use_sem); else core_child_thr(r, sz, n << 20);
    } else {
        pin_role(0);
        if (lat) core_parent_lat(r, "shm", sz, n, use_sem, NULL); else core_parent_thr(r, "shm", sz, n << 20);
    }
    if (!is_b) shm_unlink(SHM_NAME);
    print_rusage("shm");
    return 0;
}
