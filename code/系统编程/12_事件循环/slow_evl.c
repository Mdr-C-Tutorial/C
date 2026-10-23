#define _POSIX_C_SOURCE 200809L

#include "evl.h"
#include "tpool.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_PORT 21201
#define SLOW_MS 2000
#define MAX_LINE 1024
#define MAX_CONNS 256
#define MAX_DONE 256

struct conn {
    int fd;
};

struct server {
    evl_loop *loop;
    int listen_fd;
    int wake_fd;
    tpool *pool;
    int block_mode;
    int64_t start_ms;
    struct conn *conns[MAX_CONNS];
    int done[MAX_DONE];
    int done_len;
    pthread_mutex_t done_lock;
};

struct job {
    struct server *srv;
    int fd;
};

static void log_msg(struct server *srv, const char *what) {
    printf("[%6lld ms] %s\n", (long long)(evl_now_ms() - srv->start_ms), what);
    fflush(stdout);
}

static int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);

    if (flags < 0) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static struct conn *find_conn(struct server *srv, int fd) {
    if (fd < 0 || fd >= MAX_CONNS) {
        return NULL;
    }
    return srv->conns[fd];
}

static void conn_close(struct server *srv, struct conn *c) {
    evl_del_fd(srv->loop, c->fd);
    close(c->fd);
    srv->conns[c->fd] = NULL;
    free(c);
}

static void send_line(int fd, const char *line) {
    size_t len = strlen(line);
    size_t off = 0;

    while (off < len) {
        ssize_t n = write(fd, line + off, len - off);

        if (n > 0) {
            off += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        return;
    }
}

static void do_slow_work(struct server *srv, struct conn *c) {
    struct timespec ts = {SLOW_MS / 1000, (SLOW_MS % 1000) * 1000000L};

    nanosleep(&ts, NULL);
    send_line(c->fd, "slow done\n");
    log_msg(srv, "slow 处理完成");
}

static void run_slow_job(void *arg) {
    struct job *job = arg;
    struct timespec ts = {SLOW_MS / 1000, (SLOW_MS % 1000) * 1000000L};

    nanosleep(&ts, NULL);
    pthread_mutex_lock(&job->srv->done_lock);
    if (job->srv->done_len < MAX_DONE) {
        job->srv->done[job->srv->done_len++] = job->fd;
    }
    pthread_mutex_unlock(&job->srv->done_lock);
    uint64_t one = 1;

    if (write(job->srv->wake_fd, &one, sizeof one) != sizeof one) {
        perror("write eventfd");
    }
    free(job);
}

static void on_conn(evl_loop *loop, int fd, unsigned events, void *arg) {
    struct server *srv = arg;
    struct conn *c = find_conn(srv, fd);
    char buf[MAX_LINE];
    ssize_t n;

    (void)loop;
    (void)events;
    if (c == NULL) {
        return;
    }
    n = read(fd, buf, sizeof buf - 1);
    if (n <= 0) {
        conn_close(srv, c);
        return;
    }
    buf[n] = '\0';
    if (strcmp(buf, "slow\n") == 0) {
        if (srv->block_mode) {
            do_slow_work(srv, c);
            return;
        }
        struct job *job = malloc(sizeof *job);

        if (job == NULL) {
            return;
        }
        job->srv = srv;
        job->fd = fd;
        if (tpool_submit(srv->pool, run_slow_job, job) < 0) {
            free(job);
        }
        return;
    }
    for (char *p = buf; *p != '\0'; p++) {
        *p = (char)toupper((unsigned char)*p);
    }
    send_line(fd, buf);
}

static void on_wake(evl_loop *loop, int fd, unsigned events, void *arg) {
    struct server *srv = arg;
    int pending[MAX_DONE];
    int n;

    (void)loop;
    (void)events;
    uint64_t count;

    while (read(fd, &count, sizeof count) == sizeof count) {
    }
    pthread_mutex_lock(&srv->done_lock);
    n = srv->done_len;
    memcpy(pending, srv->done, (size_t)n * sizeof *pending);
    srv->done_len = 0;
    pthread_mutex_unlock(&srv->done_lock);
    for (int i = 0; i < n; i++) {
        struct conn *c = find_conn(srv, pending[i]);

        if (c != NULL) {
            send_line(c->fd, "slow done\n");
            log_msg(srv, "slow 处理完成");
        }
    }
}

static void on_accept(evl_loop *loop, int fd, unsigned events, void *arg) {
    struct server *srv = arg;

    (void)loop;
    (void)events;
    for (;;) {
        int client = accept(fd, NULL, NULL);

        if (client < 0) {
            return;
        }
        if (client >= MAX_CONNS || set_nonblock(client) < 0) {
            close(client);
            return;
        }
        struct conn *c = calloc(1, sizeof *c);

        if (c == NULL) {
            close(client);
            return;
        }
        c->fd = client;
        srv->conns[client] = c;
        if (evl_add_fd(srv->loop, client, EVL_READ, on_conn, srv) < 0) {
            conn_close(srv, c);
            return;
        }
        log_msg(srv, "接受新连接");
    }
}

static int open_listener(int port) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    int one = 1;
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons((uint16_t)port),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    };

    if (fd < 0) {
        return -1;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) < 0 ||
        bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0 ||
        listen(fd, SOMAXCONN) < 0 || set_nonblock(fd) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    struct server srv = {0};
    int argi = 1;

    if (argc > 1 && strcmp(argv[1], "--block") == 0) {
        srv.block_mode = 1;
        argi = 2;
    }
    if (argi < argc) {
        port = atoi(argv[argi]);
    }
    srv.start_ms = evl_now_ms();
    srv.loop = evl_create();
    srv.listen_fd = open_listener(port);
    if (srv.loop == NULL || srv.listen_fd < 0) {
        perror("初始化");
        return 1;
    }
    srv.wake_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (srv.wake_fd < 0) {
        perror("eventfd");
        return 1;
    }
    pthread_mutex_init(&srv.done_lock, NULL);
    if (evl_add_fd(srv.loop, srv.listen_fd, EVL_READ, on_accept, &srv) < 0 ||
        evl_add_fd(srv.loop, srv.wake_fd, EVL_READ, on_wake, &srv) < 0) {
        perror("evl_add_fd");
        return 1;
    }
    if (!srv.block_mode) {
        srv.pool = tpool_create(2);
        if (srv.pool == NULL) {
            perror("tpool_create");
            return 1;
        }
    }
    printf("slow_evl %s 监听 127.0.0.1:%d\n", srv.block_mode ? "阻塞版" : "线程池版", port);
    fflush(stdout);
    evl_run(srv.loop);
    tpool_destroy(srv.pool);
    evl_destroy(srv.loop);
    return 0;
}