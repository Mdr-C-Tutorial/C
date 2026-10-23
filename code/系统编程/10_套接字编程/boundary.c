#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BIG_SIZE (1024 * 1024)

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static long g_write_calls;

static int write_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    size_t left = len;

    while (left > 0) {
        ssize_t n = write(fd, p, left);
        g_write_calls++;
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

static void sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static int tcp_connect(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        die("socket");
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        die("connect");
    }
    return fd;
}

static void child_sender(uint16_t port, int split) {
    int fd = tcp_connect(port);

    if (!split) {
        if (write_all(fd, "hello\n", 6) < 0) {
            _exit(EXIT_FAILURE);
        }
        sleep_ms(50);
        if (write_all(fd, "world\n", 6) < 0) {
            _exit(EXIT_FAILURE);
        }
    } else {
        char *big = malloc(BIG_SIZE);
        if (big == NULL) {
            _exit(EXIT_FAILURE);
        }
        memset(big, 'x', BIG_SIZE);
        if (write_all(fd, big, BIG_SIZE) < 0) {
            _exit(EXIT_FAILURE);
        }
        printf("sender: %ld write() calls for %d bytes\n", g_write_calls,
               BIG_SIZE);
        fflush(stdout);
        free(big);
    }

    close(fd);
    _exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    if (argc != 2 || (strcmp(argv[1], "merge") != 0 &&
                      strcmp(argv[1], "split") != 0)) {
        fprintf(stderr, "usage: %s merge|split\n", argv[0]);
        return EXIT_FAILURE;
    }
    int split = strcmp(argv[1], "split") == 0;

    int lfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (lfd < 0) {
        die("socket");
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(lfd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        die("bind");
    }
    if (listen(lfd, 1) < 0) {
        die("listen");
    }
    socklen_t len = sizeof addr;
    if (getsockname(lfd, (struct sockaddr *)&addr, &len) < 0) {
        die("getsockname");
    }

    pid_t pid = fork();
    if (pid < 0) {
        die("fork");
    }
    if (pid == 0) {
        close(lfd);
        child_sender(ntohs(addr.sin_port), split);
    }

    int fd = accept(lfd, NULL, NULL);
    if (fd < 0) {
        die("accept");
    }
    close(lfd);

    if (!split) {
        sleep_ms(200);
        char buf[4096];
        ssize_t n = recv(fd, buf, sizeof buf, 0);
        if (n < 0) {
            die("recv");
        }
        printf("one recv returned %zd bytes: ", n);
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                fputs("\\n", stdout);
            } else {
                putchar(buf[i]);
            }
        }
        putchar('\n');
    } else {
        char buf[65536];
        long calls = 0;
        size_t total = 0;
        for (;;) {
            ssize_t n = recv(fd, buf, sizeof buf, 0);
            if (n < 0) {
                die("recv");
            }
            if (n == 0) {
                break;
            }
            calls++;
            total += (size_t)n;
        }
        printf("recv side: %ld calls, %zu bytes total\n", calls, total);
    }

    close(fd);
    waitpid(pid, NULL, 0);
    return EXIT_SUCCESS;
}