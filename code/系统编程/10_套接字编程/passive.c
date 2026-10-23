#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

/* 用 AI_PASSIVE 和 NULL 主机名，看看服务器该绑哪些地址。 */
int main(void) {
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo(NULL, "21000", &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return EXIT_FAILURE;
    }

    for (const struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        char host[INET6_ADDRSTRLEN];
        rc = getnameinfo(p->ai_addr, p->ai_addrlen, host, sizeof host, NULL, 0,
                         NI_NUMERICHOST);
        if (rc != 0) {
            continue;
        }
        printf("family=%d addr=%s\n", p->ai_family, host);
    }

    freeaddrinfo(res);
    return EXIT_SUCCESS;
}