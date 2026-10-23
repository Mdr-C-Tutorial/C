#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

/* 不忽略 SIGPIPE：向已经关闭的连接 send，进程被信号终止。 */
int main(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("socketpair");
        return 1;
    }
    close(sv[1]);
    ssize_t n = send(sv[0], "x", 1, 0);
    printf("send returned %ld\n", (long)n);
    return 0;
}
