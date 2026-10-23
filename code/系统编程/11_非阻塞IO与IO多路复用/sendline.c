#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static double now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        die("clock_gettime");
    }
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "用法：%s 端口 一行文本 超时秒数\n", argv[0]);
        return EXIT_FAILURE;
    }
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd == -1) {
        die("socket");
    }
    struct timeval tv = {.tv_sec = atoi(argv[3])};
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) == -1) {
        die("setsockopt");
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons((uint16_t)atoi(argv[1])),
    };
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    double start = now_ms();
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) == -1) {
        die("connect");
    }
    char line[256];
    int len = snprintf(line, sizeof line, "%s\n", argv[2]);
    if (write(fd, line, (size_t)len) != len) {
        die("write");
    }
    char reply[512];
    ssize_t n = read(fd, reply, sizeof reply - 1);
    if (n <= 0) {
        printf("没有收到应答，已等 %.1f ms\n", now_ms() - start);
        return EXIT_FAILURE;
    }
    reply[n] = '\0';
    char *nl = strchr(reply, '\n');
    if (nl != NULL) {
        *nl = '\0';
    }
    printf("应答 \"%s\"，从连接到收到应答耗时 %.1f ms\n", reply,
           now_ms() - start);
    close(fd);
    return 0;
}
