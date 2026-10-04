/* stream_bench: pipe | fifo | socketpair | unix | tcp
 *   ./stream_bench <kind> lat <msg_bytes> [iters]
 *   ./stream_bench <kind> thr <block_bytes> [total_MiB]                         */
#include "common.h"
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>

typedef struct { int r, w; } end_t;
static const char *kind;
static int p2c[2], c2p[2], sv[2], lsn = -1, tcp_port;
static char f1[128], f2[128], spath[128];

static void prepare(void) {
    int me = (int)getpid();
    if (!strcmp(kind, "pipe")) {
        if (pipe(p2c) || pipe(c2p)) DIE("pipe");
    } else if (!strcmp(kind, "fifo")) {
        snprintf(f1, sizeof f1, "/tmp/ipc_f1_%d", me); snprintf(f2, sizeof f2, "/tmp/ipc_f2_%d", me);
        if (mkfifo(f1, 0600) || mkfifo(f2, 0600)) DIE("mkfifo");
    } else if (!strcmp(kind, "socketpair")) {
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) DIE("socketpair");
    } else if (!strcmp(kind, "unix")) {
        snprintf(spath, sizeof spath, "/tmp/ipc_s_%d.sock", me);
        struct sockaddr_un a = { .sun_family = AF_UNIX };
        strncpy(a.sun_path, spath, sizeof a.sun_path - 1);
        unlink(spath);
        lsn = socket(AF_UNIX, SOCK_STREAM, 0);
        if (bind(lsn, (struct sockaddr *)&a, sizeof a) || listen(lsn, 1)) DIE("unix bind/listen");
    } else if (!strcmp(kind, "tcp")) {
        struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = 0 };
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        lsn = socket(AF_INET, SOCK_STREAM, 0);
        int one = 1; setsockopt(lsn, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        if (bind(lsn, (struct sockaddr *)&a, sizeof a) || listen(lsn, 1)) DIE("tcp bind/listen");
        socklen_t l = sizeof a; getsockname(lsn, (struct sockaddr *)&a, &l);
        tcp_port = ntohs(a.sin_port);
    } else { fprintf(stderr, "unknown kind %s\n", kind); exit(2); }
}

static end_t open_end(int role) {
    end_t e = { -1, -1 };
    if (!strcmp(kind, "pipe")) {
        if (role == 0) { close(p2c[0]); close(c2p[1]); e.r = c2p[0]; e.w = p2c[1]; }
        else           { close(p2c[1]); close(c2p[0]); e.r = p2c[0]; e.w = c2p[1]; }
    } else if (!strcmp(kind, "fifo")) {
        /* open() блокується до появи другої сторони - порядок важливий (уникаємо deadlock) */
        if (role == 0) { e.w = open(f1, O_WRONLY); e.r = open(f2, O_RDONLY); }
        else           { e.r = open(f1, O_RDONLY); e.w = open(f2, O_WRONLY); }
        if (e.r < 0 || e.w < 0) DIE("fifo open");
    } else if (!strcmp(kind, "socketpair")) {
        int fd = role == 0 ? sv[0] : sv[1];
        close(role == 0 ? sv[1] : sv[0]);
        e.r = e.w = fd;
    } else if (!strcmp(kind, "unix")) {
        int fd;
        if (role == 0) { fd = accept(lsn, NULL, NULL); }
        else {
            struct sockaddr_un a = { .sun_family = AF_UNIX };
            strncpy(a.sun_path, spath, sizeof a.sun_path - 1);
            fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (connect(fd, (struct sockaddr *)&a, sizeof a)) DIE("connect");
        }
        close(lsn); e.r = e.w = fd;
    } else {
        int fd, one = 1;
        if (role == 0) { fd = accept(lsn, NULL, NULL); }
        else {
            struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(tcp_port) };
            a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            fd = socket(AF_INET, SOCK_STREAM, 0);
            if (connect(fd, (struct sockaddr *)&a, sizeof a)) DIE("connect");
        }
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one); /* критично для latency */
        close(lsn); e.r = e.w = fd;
    }
    return e;
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s <pipe|fifo|socketpair|unix|tcp> <lat|thr> <size> [n]\n", argv[0]); return 2; }
    kind = argv[1];
    int lat = !strcmp(argv[2], "lat");
    size_t sz = (size_t)atoll(argv[3]);
    if (sz < 8) sz = 8;                        /* перші 8 байт = лічильник для перевірки */
    size_t n = argc > 4 ? (size_t)atoll(argv[4]) : (lat ? 200000 : 1024);
    size_t total = n << 20, nblk = total / sz;
    prepare();
    pid_t pid = fork();
    if (pid < 0) DIE("fork");
    char *buf = aligned_alloc(64, (sz + 63) & ~63ul);
    memset(buf, 0xAB, sz);
    if (pid == 0) {                             /* ---- дитина ---- */
        pin_role(1);
        end_t e = open_end(1);
        if (lat) {
            for (size_t i = 0; i < n + WARMUP; i++) { read_full(e.r, buf, sz); write_full(e.w, buf, sz); }
        } else {
            for (size_t b = 0; b < nblk; b++) {
                read_full(e.r, buf, sz);
                uint64_t seq; memcpy(&seq, buf, 8);
                if (seq != b) { fprintf(stderr, "DATA ERROR at block %zu (got %llu)\n", b, (unsigned long long)seq); exit(3); }
            }
            char ack = 1; write_full(e.w, &ack, 1);
        }
        _exit(0);
    }
    pin_role(0);                                /* ---- батько ---- */
    end_t e = open_end(0);
    if (lat) {
        uint64_t *s = malloc(n * sizeof *s);
        for (size_t i = 0; i < n + WARMUP; i++) {
            uint64_t seq = i; memcpy(buf, &seq, 8);
            uint64_t t0 = now_ns();
            write_full(e.w, buf, sz);
            read_full(e.r, buf, sz);
            uint64_t t1 = now_ns();
            memcpy(&seq, buf, 8);
            if (seq != i) { fprintf(stderr, "DATA ERROR in echo\n"); return 3; }
            if (i >= WARMUP) s[i - WARMUP] = (t1 - t0) / 2;
        }
        print_lat(kind, sz, s, n);
    } else {
        uint64_t t0 = now_ns();
        for (size_t b = 0; b < nblk; b++) { uint64_t seq = b; memcpy(buf, &seq, 8); write_full(e.w, buf, sz); }
        char ack; read_full(e.r, &ack, 1);       /* чекаємо, поки споживач отримав усе */
        print_thr(kind, sz, nblk * sz, now_ns() - t0);
    }
    int st; waitpid(pid, &st, 0);
    if (f1[0]) { unlink(f1); unlink(f2); }
    if (spath[0]) unlink(spath);
    print_rusage(kind);
    return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}
