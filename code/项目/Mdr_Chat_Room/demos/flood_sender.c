#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* 登记昵称后连续发送 COUNT 条 1024 字节的 SAY，把别人的发送缓冲区灌满。
 * 用法：flood_sender PORT NICK COUNT */
int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s PORT NICK COUNT\n", argv[0]);
        return 2;
    }
    if (strlen(argv[2]) > 16) {
        fprintf(stderr, "nick too long\n");
        return 2;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)atoi(argv[1]));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("connect");
        return 1;
    }

    size_t nick_len = strlen(argv[2]);
    unsigned char nick[3 + 1 + 16];
    nick[0] = 0x01; /* NICK */
    nick[1] = 0x00;
    nick[2] = (unsigned char)(1 + nick_len);
    nick[3] = (unsigned char)nick_len;
    memcpy(nick + 4, argv[2], nick_len);
    if (send(fd, nick, 4 + nick_len, 0) < 0) {
        perror("send");
        return 1;
    }

    /* SAY，正文 1024 个 'x'：body_len = 1 + 0 + 1024 = 1025 = 0x0401 */
    unsigned char frame[3 + 1 + 1024];
    memset(frame, 'x', sizeof frame);
    frame[0] = 0x02;
    frame[1] = 0x04;
    frame[2] = 0x01;
    frame[3] = 0x00;

    int count = atoi(argv[3]);
    for (int i = 0; i < count; i++) {
        const unsigned char *p = frame;
        size_t left = sizeof frame;
        while (left > 0) {
            ssize_t n = send(fd, p, left, 0);
            if (n < 0) {
                perror("send");
                return 1;
            }
            p += n;
            left -= (size_t)n;
        }
    }
    printf("sent %d frames\n", count);
    sleep(1);
    close(fd);
    return 0;
}
