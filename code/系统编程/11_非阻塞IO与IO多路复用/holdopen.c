#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "用法：%s 端口 秒数\n", argv[0]);
        return EXIT_FAILURE;
    }
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons((uint16_t)atoi(argv[1])),
    };
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) == -1) {
        perror("connect");
        return EXIT_FAILURE;
    }
    puts("connected, staying idle");
    fflush(stdout);
    sleep((unsigned)atoi(argv[2]));
    close(fd);
    puts("closed");
    return 0;
}
