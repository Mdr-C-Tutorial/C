#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* 用带缓冲的读写做请求-应答：每条命令一行，读回一行。 */
struct stream {
    int fd;
    char buf[4096];
    size_t len;
    size_t pos;
};

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void fill(struct stream *s) {
    memmove(s->buf, s->buf + s->pos, s->len - s->pos);
    s->len -= s->pos;
    s->pos = 0;
    ssize_t n = read(s->fd, s->buf + s->len, sizeof s->buf - s->len);
    if (n <= 0) {
        fprintf(stderr, "connection closed\n");
        exit(EXIT_FAILURE);
    }
    s->len += (size_t)n;
}

static void read_line(struct stream *s, char *out, size_t cap) {
    for (;;) {
        char *nl = memchr(s->buf + s->pos, '\n', s->len - s->pos);
        if (nl != NULL) {
            size_t len = (size_t)(nl - (s->buf + s->pos)) + 1;
            if (len >= cap) {
                len = cap - 1;
            }
            memcpy(out, s->buf + s->pos, len);
            out[len] = '\0';
            s->pos += len;
            return;
        }
        fill(s);
    }
}

static void send_all(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t n = write(fd, buf, len);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            die("write");
        }
        buf += n;
        len -= (size_t)n;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "用法：%s 端口 客户端号\n", argv[0]);
        return EXIT_FAILURE;
    }
    int port = atoi(argv[1]);
    int id = atoi(argv[2]);

    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd == -1) {
        die("socket");
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons((uint16_t)port),
    };
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        die("inet_pton");
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) == -1) {
        die("connect");
    }

    struct stream s = {.fd = fd};
    char line[256];
    char reply[512];
    int bad = 0;

    for (int i = 0; i < 100; i++) {
        snprintf(line, sizeof line, "SET c%d_k%d v%d\n", id, i, i);
        send_all(fd, line, strlen(line));
        read_line(&s, reply, sizeof reply);
        if (strcmp(reply, "OK\n") != 0) {
            fprintf(stderr, "client %d: SET 返回 %s", id, reply);
            bad++;
        }

        snprintf(line, sizeof line, "GET c%d_k%d\n", id, i);
        send_all(fd, line, strlen(line));
        read_line(&s, reply, sizeof reply);
        snprintf(line, sizeof line, "VALUE v%d\n", i);
        if (strcmp(reply, line) != 0) {
            fprintf(stderr, "client %d: GET 得到 %s，期望 %s", id, reply, line);
            bad++;
        }
    }
    close(fd);
    if (bad != 0) {
        printf("client %d：%d 条不符\n", id, bad);
        return EXIT_FAILURE;
    }
    printf("client %d：200 条命令全部正确\n", id);
    return 0;
}