#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_LINE 1024

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 收到一行，返回它的大写形式。quit 时返回 0。 */
static size_t upper_line(const char *line, size_t len, char *out) {
    if (len == 5 && memcmp(line, "quit\n", 5) == 0) {
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        char ch = line[i];
        out[i] = (ch >= 'a' && ch <= 'z') ? (char)(ch - 'a' + 'A') : ch;
    }
    return len;
}

/* 阻塞式地一次服务一个连接：读满一行回一行，直到对方关闭或发 quit。 */
static void serve(int fd) {
    char buf[MAX_LINE];
    char out[MAX_LINE];
    size_t len = 0;
    for (;;) {
        ssize_t n = read(fd, buf + len, sizeof buf - len);
        if (n == 0) {
            return;
        }
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        len += (size_t)n;
        size_t start = 0;
        for (;;) {
            char *nl = memchr(buf + start, '\n', len - start);
            if (nl == NULL) {
                break;
            }
            size_t line_len = (size_t)(nl - (buf + start)) + 1;
            size_t out_len = upper_line(buf + start, line_len, out);
            if (out_len == 0) {
                return;
            }
            ssize_t w = write(fd, out, out_len);
            if (w != (ssize_t)out_len) {
                return;
            }
            start += line_len;
        }
        memmove(buf, buf + start, len - start);
        len -= start;
    }
}

int main(int argc, char *argv[]) {
    const char *port = argc > 1 ? argv[1] : "21100";
    int listener = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listener == -1) {
        die("socket");
    }
    int one = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) ==
        -1) {
        die("setsockopt");
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons((uint16_t)atoi(port)),
    };
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        die("inet_pton");
    }
    if (bind(listener, (struct sockaddr *)&addr, sizeof addr) == -1) {
        die("bind");
    }
    if (listen(listener, 16) == -1) {
        die("listen");
    }

    for (;;) {
        int fd = accept(listener, NULL, NULL);
        if (fd == -1) {
            if (errno == EINTR) {
                continue;
            }
            die("accept");
        }
        serve(fd);
        close(fd);
    }
}