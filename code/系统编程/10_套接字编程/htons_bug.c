#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

/* 故意省略 htons：看看内核把 21000 当成什么。 */
int main(void) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 21000;
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind");
        close(fd);
        return EXIT_FAILURE;
    }

    socklen_t len = sizeof addr;
    if (getsockname(fd, (struct sockaddr *)&addr, &len) < 0) {
        perror("getsockname");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("kernel reports port %u (0x%04x)\n", ntohs(addr.sin_port),
           ntohs(addr.sin_port));
    close(fd);
    return EXIT_SUCCESS;
}