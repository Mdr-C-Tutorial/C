#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

/* recv 的长度参数为 0 时返回 0，与“对方关闭了连接”的返回值相同。 */
int main(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("socketpair");
        return 1;
    }
    char byte = 'x';
    if (send(sv[0], &byte, 1, 0) != 1) {
        perror("send");
        return 1;
    }
    char buf[8];
    ssize_t n = recv(sv[1], buf, 0, 0);
    printf("recv(fd, buf, 0, 0) = %ld\n", (long)n);
    n = recv(sv[1], buf, sizeof buf, 0);
    printf("recv(fd, buf, sizeof buf, 0) = %ld, 读到的字节是 %c\n", (long)n,
           buf[0]);
    close(sv[0]);
    close(sv[1]);
    return 0;
}
