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
    int a = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    int b = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (a < 0 || b < 0) {
        die("socket");
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(a, (struct sockaddr *)&addr, sizeof addr) < 0) {
        die("bind");
    }
    socklen_t len = sizeof addr;
    if (getsockname(a, (struct sockaddr *)&addr, &len) < 0) {
        die("getsockname");
    }
    if (connect(b, (struct sockaddr *)&addr, sizeof addr) < 0) {
        die("connect");
    }

    const char *msg = "0123456789";
    if (send(b, msg, 10, 0) != 10) {
        die("send");
    }

    char small[4];
    ssize_t n = recv(a, small, sizeof small, 0);
    printf("datagram of 10 bytes read into a 4-byte buffer: recv returned %zd\n",
           n);
    printf("last byte in buffer: '%c'\n", small[3]);

    char rest[64];
    n = recv(a, rest, sizeof rest, MSG_DONTWAIT);
    if (n < 0) {
        printf("second recv: %s (nothing left)\n", strerror(errno));
    } else {
        printf("second recv: %zd bytes\n", n);
    }

    close(a);
    close(b);
    return EXIT_SUCCESS;
}