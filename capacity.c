/* Місткість каналів: пишемо без читання в non-blocking режимі до EAGAIN.
 * CSV: capacity,<канал>,<розмір_запису>,<байт_або_повідомлень>,<примітка>  */
#include "common.h"
#include <fcntl.h>
#include <mqueue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "shared_core.h"

static size_t fill(int fd, size_t chunk) {
    char *b = calloc(1, chunk); size_t tot = 0;
    for (;;) {
        ssize_t w = write(fd, b, chunk);
        if (w < 0) { if (errno == EINTR) continue; break; }   /* EAGAIN - канал повний */
        tot += (size_t)w;
    }
    free(b); return tot;
}
static void nb(int fd) { fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK); }

int main(void) {
    int fd[2];
    /* pipe */
    for (size_t ch = 1; ch <= 4096; ch *= 4096) {
        if (pipe(fd)) DIE("pipe"); nb(fd[1]);
        printf("capacity,pipe,%zu,%zu,F_GETPIPE_SZ=%d\n", ch, fill(fd[1], ch), fcntl(fd[1], F_GETPIPE_SZ));
        close(fd[0]); close(fd[1]);
    }
    if (pipe(fd)) DIE("pipe"); nb(fd[1]);
    int ok = fcntl(fd[1], F_SETPIPE_SZ, 1 << 20);
    printf("capacity,pipe_setpipe_1MiB,4096,%zu,F_SETPIPE_SZ ret=%d\n", fill(fd[1], 4096), ok);
    close(fd[0]); close(fd[1]);
    /* fifo */
    char fp[64]; snprintf(fp, sizeof fp, "/tmp/ipc_cap_%d", (int)getpid());
    mkfifo(fp, 0600);
    int fr = open(fp, O_RDONLY | O_NONBLOCK), fw = open(fp, O_WRONLY | O_NONBLOCK);
    printf("capacity,fifo,4096,%zu,\n", fill(fw, 4096)); close(fr); close(fw); unlink(fp);
    /* socketpair stream / dgram */
    int types[2] = { SOCK_STREAM, SOCK_DGRAM };
    const char *tn[2] = { "unix_stream", "unix_dgram" };
    for (int t = 0; t < 2; t++) {
        size_t chunks[2] = { 1, 1024 };
        for (int c = 0; c < 2; c++) {
            int sv[2]; if (socketpair(AF_UNIX, types[t], 0, sv)) DIE("socketpair"); nb(sv[0]);
            int snd = 0, rcv = 0; socklen_t l = sizeof snd;
            getsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &snd, &l); getsockopt(sv[1], SOL_SOCKET, SO_RCVBUF, &rcv, &l);
            size_t bytes = fill(sv[0], chunks[c]);
            printf("capacity,%s,%zu,%zu,SO_SNDBUF=%d SO_RCVBUF=%d%s\n", tn[t], chunks[c], bytes, snd, rcv,
                   t == 1 ? " (рахується в байтах; повідомлень=bytes/chunk)" : "");
            close(sv[0]); close(sv[1]);
        }
    }
    /* POSIX mqueue */
    char qn[64]; snprintf(qn, sizeof qn, "/ipc_cap_%d", (int)getpid());
    struct mq_attr at = { .mq_maxmsg = 10, .mq_msgsize = 64 };
    mqd_t q = mq_open(qn, O_CREAT | O_RDWR | O_NONBLOCK, 0600, &at);
    if (q != (mqd_t)-1) {
        char m[64] = {0}; size_t cnt = 0; while (mq_send(q, m, sizeof m, 0) == 0) cnt++;
        printf("capacity,mqueue,64,%zu,maxmsg=10 -> %zu повідомлень = %zu байт\n", cnt * 64, cnt, cnt * 64);
        mq_close(q); mq_unlink(qn);
    } else perror("mq_open");
    /* mmap / shm */
    printf("capacity,mmap_shm_region,0,%zu,розмір відображення (ring-буфер даних=%u)\n", sizeof(region_t), RING_SIZE);
    return 0;
}
