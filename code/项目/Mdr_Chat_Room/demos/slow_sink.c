#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* 只登记昵称、之后一直不读的客户端，用来把服务器的发送缓冲区堆满。
 * 用法：slow_sink PORT NICK */
int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s PORT NICK\n", argv[0]);
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
    /* 接收缓冲区设小，几百 KB 的数据就能把发送方的缓冲区堆满 */
    int small = 4096;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &small, sizeof small);

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
    unsigned char frame[3 + 1 + 16];
    frame[0] = 0x01; /* NICK */
    frame[1] = 0x00;
    frame[2] = (unsigned char)(1 + nick_len);
    frame[3] = (unsigned char)nick_len;
    memcpy(frame + 4, argv[2], nick_len);
    if (send(fd, frame, 4 + nick_len, 0) < 0) {
        perror("send");
        return 1;
    }

    printf("registered as %s, not reading\n", argv[2]);
    fflush(stdout);
    sleep(10);
    close(fd);
    return 0;
}
