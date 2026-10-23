#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define LINE_CAP 4096

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void die_rc(int rc, const char *what) {
    fprintf(stderr, "%s: %s\n", what, strerror(rc));
    exit(EXIT_FAILURE);
}

static int write_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    size_t left = len;

    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        p += n;
        left -= (size_t)n;
    }
    return 0;
}

static int tcp_listen(const char *port) {
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo("127.0.0.1", port, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        exit(EXIT_FAILURE);
    }

    int lfd = -1;
    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        lfd = socket(p->ai_family, p->ai_socktype | SOCK_CLOEXEC,
                     p->ai_protocol);
        if (lfd < 0) {
            continue;
        }
        int one = 1;
        if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) < 0) {
            die("setsockopt");
        }
        if (bind(lfd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        close(lfd);
        lfd = -1;
    }
    freeaddrinfo(res);

    if (lfd < 0) {
        die("bind");
    }
    if (listen(lfd, 16) < 0) {
        die("listen");
    }
    return lfd;
}

static void handle_connection(int fd) {
    char buf[LINE_CAP];
    size_t used = 0;

    for (;;) {
        ssize_t n = recv(fd, buf + used, sizeof buf - used, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        if (n == 0) {
            return;
        }
        used += (size_t)n;

        size_t start = 0;
        for (size_t i = 0; i < used; i++) {
            if (buf[i] != '\n') {
                continue;
            }
            buf[i] = '\0';
            if (strcmp(buf + start, "quit") == 0) {
                return;
            }
            for (char *q = buf + start; *q != '\0'; q++) {
                *q = (char)toupper((unsigned char)*q);
            }
            if (write_all(fd, buf + start, strlen(buf + start)) < 0 ||
                write_all(fd, "\n", 1) < 0) {
                return;
            }
            start = i + 1;
        }
        memmove(buf, buf + start, used - start);
        used -= start;
        if (used == sizeof buf) {
            return;
        }
    }
}

/* 线程的入口参数：描述符按值传进来，由线程负责关闭。 */
struct conn_args {
    int fd;
};

static void *connection_thread(void *arg) {
    struct conn_args *ca = arg;
    int fd = ca->fd;
    free(ca);

    handle_connection(fd);
    close(fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    const char *port = argc > 1 ? argv[1] : "21000";

    signal(SIGPIPE, SIG_IGN);

    int lfd = tcp_listen(port);
    printf("listening on 127.0.0.1 port %s (one thread per connection)\n",
           port);
    fflush(stdout);

    pthread_attr_t attr;
    int rc = pthread_attr_init(&attr);
    if (rc != 0) {
        die_rc(rc, "pthread_attr_init");
    }
    rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (rc != 0) {
        die_rc(rc, "pthread_attr_setdetachstate");
    }

    for (;;) {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR) {
                continue;
            }
            die("accept");
        }

        struct conn_args *ca = malloc(sizeof *ca);
        if (ca == NULL) {
            die("malloc");
        }
        ca->fd = cfd;

        pthread_t tid;
        rc = pthread_create(&tid, &attr, connection_thread, ca);
        if (rc != 0) {
            close(cfd);
            free(ca);
            fprintf(stderr, "pthread_create: %s\n", strerror(rc));
        }
    }
}