/* POSIX message queue:  ./mq_bench lat <msg> [iters]   |   ./mq_bench thr <block> [total_MiB]
 * Обмеження: msg <= /proc/sys/fs/mqueue/msgsize_max (8192 за замовчуванням). */
#include "common.h"
#include <mqueue.h>
#include <fcntl.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <lat|thr> <size> [n]\n", argv[0]); return 2; }
    int lat = !strcmp(argv[1], "lat");
    size_t sz = (size_t)atoll(argv[2]); if (sz < 8) sz = 8;
    size_t n = argc > 3 ? (size_t)atoll(argv[3]) : (lat ? 100000 : 256);
    size_t nblk = (n << 20) / sz;
    char q1n[64], q2n[64];
    snprintf(q1n, sizeof q1n, "/ipc_q1_%d", (int)getpid()); snprintf(q2n, sizeof q2n, "/ipc_q2_%d", (int)getpid());
    mq_unlink(q1n); mq_unlink(q2n);
    struct mq_attr at = { .mq_maxmsg = 10, .mq_msgsize = (long)sz };
    mqd_t q1 = mq_open(q1n, O_CREAT | O_RDWR, 0600, &at);
    mqd_t q2 = mq_open(q2n, O_CREAT | O_RDWR, 0600, &at);
    if (q1 == (mqd_t)-1 || q2 == (mqd_t)-1) {
        perror("mq_open (перевір /proc/sys/fs/mqueue/{msg_max,msgsize_max} та ulimit -q)"); return 1;
    }
    char *buf = calloc(1, sz);
    pid_t pid = fork();
    if (pid == 0) {
        pin_role(1);
        if (lat) {
            for (size_t i = 0; i < n + WARMUP; i++) { mq_receive(q1, buf, sz, NULL); mq_send(q2, buf, sz, 0); }
        } else {
            for (size_t b = 0; b < nblk; b++) {
                if (mq_receive(q1, buf, sz, NULL) < 0) DIE("mq_receive");
                uint64_t seq; memcpy(&seq, buf, 8);
                if (seq != b) { fprintf(stderr, "DATA ERROR\n"); _exit(3); }
            }
            mq_send(q2, buf, sz, 0);
        }
        _exit(0);
    }
    pin_role(0);
    if (lat) {
        uint64_t *s = malloc(n * sizeof *s);
        for (size_t i = 0; i < n + WARMUP; i++) {
            uint64_t seq = i; memcpy(buf, &seq, 8);
            uint64_t t0 = now_ns();
            if (mq_send(q1, buf, sz, 0)) DIE("mq_send");
            mq_receive(q2, buf, sz, NULL);
            uint64_t t1 = now_ns();
            if (i >= WARMUP) s[i - WARMUP] = (t1 - t0) / 2;
        }
        print_lat("mqueue", sz, s, n);
    } else {
        uint64_t t0 = now_ns();
        for (size_t b = 0; b < nblk; b++) { uint64_t seq = b; memcpy(buf, &seq, 8); if (mq_send(q1, buf, sz, 0)) DIE("mq_send"); }
        mq_receive(q2, buf, sz, NULL);
        print_thr("mqueue", sz, nblk * sz, now_ns() - t0);
    }
    int st; waitpid(pid, &st, 0);
    mq_unlink(q1n); mq_unlink(q2n);
    print_rusage("mqueue");
    return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}
