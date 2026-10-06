/* mmap у різних режимах.
 *   ./mmap_bench <variant> lat <msg> <iters> [spin|sem]
 *   ./mmap_bench <variant> thr <block> <total_MiB>
 *   ./mmap_bench <variant> pf  <MiB> (вартість page fault при першому/другому дотику)
 *   ./mmap_bench <variant> demo (видимість запису між батьком і дитиною)
 * variants: anon, anon_populate, anon_hugetlb, anon_private,
 * file, file_populate, file_private, file_msync_async, file_msync_sync
 * (lat/thr потребують MAP_SHARED: *_private працюють лише в pf/demo)*/
#include "common.h"
#include "shared_core.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

static const char *g_var;
static int g_fd = -1;
static char g_path[256];
static void *g_base; static size_t g_synclen; static int g_syncflag;

static int has(const char *s) { return strstr(g_var, s) != NULL; }
static void do_msync(void) { if (msync(g_base, g_synclen, g_syncflag)) perror("msync"); }

static void *map_variant(size_t len, int prefill) {
    int flags = has("private") ? MAP_PRIVATE : MAP_SHARED;
    if (has("populate")) flags |= MAP_POPULATE;
    int fd = -1;
    if (!strncmp(g_var, "file", 4)) {
        const char *p = getenv("MMAP_PATH");
        snprintf(g_path, sizeof g_path, "%s", p ? p : "./ipc_mmap.dat");
        fd = open(g_path, O_RDWR | O_CREAT | O_TRUNC, 0600);
        if (fd < 0) DIE("open");
        if (ftruncate(fd, (off_t)len)) DIE("ftruncate");
        if (prefill) {// щоб сторінки вже були в page cache
            char *z = calloc(1, 1 << 20); memset(z, 'a', 1 << 20);
            for (size_t o = 0; o < len; o += 1 << 20) if (pwrite(fd, z, 1 << 20, (off_t)o) < 0) DIE("pwrite");
            free(z);
        }
        g_fd = fd;
    } else {
        flags |= MAP_ANONYMOUS;
        if (has("hugetlb")) flags |= MAP_HUGETLB;
    }
    void *p = mmap(NULL, len, PROT_READ | PROT_WRITE, flags, fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap"); if (has("hugetlb")) fprintf(stderr, "hugetlb: потрібні hugepages (/proc/sys/vm/nr_hugepages)\n");
        exit(1);
    }
    return p;
}

static void cleanup(void) { if (g_path[0]) unlink(g_path); }

static void mode_pf(size_t mib) {
    size_t len = mib << 20, pages = len / 4096;
    struct rusage r0, r1, r2;
    getrusage(RUSAGE_SELF, &r0);
    uint64_t t0 = now_ns();
    volatile char *p = map_variant(len, 1);
    uint64_t t1 = now_ns();
    for (size_t i = 0; i < pages; i++) p[i * 4096] = 1;// перший запис у кожну сторінку
    uint64_t t2 = now_ns();
    getrusage(RUSAGE_SELF, &r1);
    for (size_t i = 0; i < pages; i++) p[i * 4096] = 2;// другий прохід - без fault
    uint64_t t3 = now_ns();
    getrusage(RUSAGE_SELF, &r2);
    // name,pf,pages,mmap_us,first_touch_ns_per_page,second_touch_ns_per_page,minor_faults_first_pass
    printf("mmap_%s,pf,%zu,%.1f,%.1f,%.1f,%ld\n", g_var, pages, (double)(t1 - t0) / 1e3,
           (double)(t2 - t1) / (double)pages, (double)(t3 - t2) / (double)pages, r1.ru_minflt - r0.ru_minflt);
    cleanup();
}

static void mode_demo(void) {
    size_t len = 4096;
    volatile char *p = map_variant(len, 1);
    p[0] = 'A';
    pid_t pid = fork();
    if (pid == 0) { p[0] = 'C'; _exit(0); }// дитина змінює свою копію/спільну сторінку
    waitpid(pid, NULL, 0);
    printf("variant=%s: батько після запису дитини бачить '%c' (%s)\n", g_var, p[0],
           p[0] == 'C' ? "SHARED: зміна видима - працює як IPC" : "PRIVATE: copy-on-write, зміна НЕ видима - не IPC");
    if (g_fd >= 0) {
        char c = 0; if (pread(g_fd, &c, 1, 0) < 0) perror("pread");
        printf("   вміст файлу на диску/в page cache: '%c' (%s)\n", c,
               has("private") ? "MAP_PRIVATE ніколи не пише у файл" : "MAP_SHARED пише у файл");
    }
    cleanup();
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: see header of mmap_bench.c\n"); return 2; }
    g_var = argv[1];
    const char *mode = argv[2];
    if (!strcmp(mode, "pf"))   { mode_pf(argc > 3 ? (size_t)atoll(argv[3]) : 256); return 0; }
    if (!strcmp(mode, "demo")) { mode_demo(); return 0; }
    if (has("private")) { fprintf(stderr, "MAP_PRIVATE не є IPC: використай режим demo або pf\n"); return 2; }
    if (argc < 5) { fprintf(stderr, "потрібно: <variant> <lat|thr> <size> <n> [spin|sem]\n"); return 2; }
    int lat = !strcmp(mode, "lat");
    size_t sz = (size_t)atoll(argv[3]), n = (size_t)atoll(argv[4]);
    int use_sem = argc > 5 && !strcmp(argv[5], "sem");
    char name[128]; snprintf(name, sizeof name, "mmap_%s", g_var);

    size_t len = region_bytes();
    region_t *r = map_variant(len, 0);
    g_base = r; g_synclen = offsetof(region_t, head);
    void (*hook)(void) = NULL;
    if (has("msync_async")) { g_syncflag = MS_ASYNC; hook = do_msync; }
    if (has("msync_sync"))  { g_syncflag = MS_SYNC;  hook = do_msync; }
    region_init(r);

    pid_t pid = fork();
    if (pid < 0) DIE("fork");
    if (pid == 0) {
        pin_role(1);
        if (lat) core_child_lat(r, n, use_sem); else core_child_thr(r, sz, n << 20);
        _exit(0);
    }
    pin_role(0);
    if (lat) core_parent_lat(r, name, sz, n, use_sem, hook);
    else {
        core_parent_thr(r, name, sz, n << 20);
        if (hook) { uint64_t t = now_ns(); msync(r, len, MS_SYNC);
                    fprintf(stderr, "[info] фінальний msync(MS_SYNC) усього регіону: %.2f ms\n", (double)(now_ns() - t) / 1e6); }
    }
    waitpid(pid, NULL, 0);
    print_rusage(name);
    cleanup();
    return 0;
}
