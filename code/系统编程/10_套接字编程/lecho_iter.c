#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
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

static int tcp_listen(const char *host, const char *port, int reuse) {
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo(host, port, &hints, &res);
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
        if (reuse) {
            int one = 1;
            if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) <
                0) {
                die("setsockopt");
            }
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

static void print_peer(const struct sockaddr_storage *peer, socklen_t len) {
    char host[INET6_ADDRSTRLEN];
    char serv[16];
    int rc = getnameinfo((const struct sockaddr *)peer, len, host, sizeof host,
                         serv, sizeof serv, NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc != 0) {
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(rc));
        return;
    }
    printf("connection from %s port %s\n", host, serv);
    fflush(stdout);
}

static int handle_connection(int fd) {
    char buf[LINE_CAP];
    size_t used = 0;

    for (;;) {
        ssize_t n = recv(fd, buf + used, sizeof buf - used, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return 0;
        }
        used += (size_t)n;

        size_t start = 0;
        for (size_t i = 0; i < used; i++) {
            if (buf[i] != '\n') {
                continue;
            }
            buf[i] = '\0';
            if (strcmp(buf + start, "quit") == 0) {
                return 0;
            }
            for (char *q = buf + start; *q != '\0'; q++) {
                *q = (char)toupper((unsigned char)*q);
            }
            if (write_all(fd, buf + start, strlen(buf + start)) < 0 ||
                write_all(fd, "\n", 1) < 0) {
                return -1;
            }
            start = i + 1;
        }
        memmove(buf, buf + start, used - start);
        used -= start;
        if (used == sizeof buf) {
            return -1;
        }
    }
}

int main(int argc, char *argv[]) {
    const char *port = argc > 1 ? argv[1] : "21000";
    int reuse = !(argc > 2 && strcmp(argv[2], "noreuse") == 0);

    signal(SIGPIPE, SIG_IGN);

    int lfd = tcp_listen("127.0.0.1", port, reuse);
    printf("listening on 127.0.0.1 port %s\n", port);
    fflush(stdout);

    for (;;) {
        struct sockaddr_storage peer;
        socklen_t peer_len = sizeof peer;
        int cfd = accept(lfd, (struct sockaddr *)&peer, &peer_len);
        if (cfd < 0) {
            if (errno == EINTR) {
                continue;
            }
            die("accept");
        }
        print_peer(&peer, peer_len);
        handle_connection(cfd);
        close(cfd);
    }
}