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

/* 接受一个连接，读一次就关闭：用来演示向已关闭连接写入的后果。 */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s PORT\n", argv[0]);
        return EXIT_FAILURE;
    }

    int lfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (lfd < 0) {
        die("socket");
    }
    int one = 1;
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) < 0) {
        die("setsockopt");
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)atoi(argv[1]));
    if (bind(lfd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        die("bind");
    }
    if (listen(lfd, 1) < 0) {
        die("listen");
    }

    int cfd = accept(lfd, NULL, NULL);
    if (cfd < 0) {
        die("accept");
    }
    char buf[4096];
    ssize_t n = recv(cfd, buf, sizeof buf, 0);
    printf("server: read %zd bytes, then closes the connection\n", n);
    fflush(stdout);
    close(cfd);
    close(lfd);
    return EXIT_SUCCESS;
}