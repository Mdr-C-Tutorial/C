#define _POSIX_C_SOURCE 200809L

#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s HOST PORT\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo(argv[1], argv[2], &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return EXIT_FAILURE;
    }

    for (const struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        char host[INET6_ADDRSTRLEN];
        char serv[16];
        rc = getnameinfo(p->ai_addr, p->ai_addrlen, host, sizeof host, serv,
                         sizeof serv, NI_NUMERICHOST | NI_NUMERICSERV);
        if (rc != 0) {
            fprintf(stderr, "getnameinfo: %s\n", gai_strerror(rc));
            continue;
        }
        const char *family = "other";
        if (p->ai_family == AF_INET) {
            family = "AF_INET";
        } else if (p->ai_family == AF_INET6) {
            family = "AF_INET6";
        }
        printf("family=%-7s socktype=%d protocol=%2d address=%s port=%s\n",
               family, p->ai_socktype, p->ai_protocol, host, serv);
    }

    freeaddrinfo(res);
    return EXIT_SUCCESS;
}