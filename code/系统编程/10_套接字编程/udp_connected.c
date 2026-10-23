#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        die("socket");
    }

    /* connect 一个本机没人监听的 UDP 端口：connect 本身不检查对端是否存在。 */
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(21099);
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        die("connect");
    }
    puts("connect() succeeded without any listener");

    const char *msg = "hello";
    if (send(fd, msg, strlen(msg), 0) < 0) {
        die("send");
    }

    char buf[64];
    ssize_t n = recv(fd, buf, sizeof buf, 0);
    if (n < 0) {
        printf("recv: %s\n", strerror(errno));
    } else {
        printf("recv: %zd bytes\n", n);
    }

    close(fd);
    return EXIT_SUCCESS;
}