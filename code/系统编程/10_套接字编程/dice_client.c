#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static int udp_client_socket(const char *host, const char *port,
                             struct addrinfo **res_out) {
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int rc = getaddrinfo(host, port, &hints, res_out);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        exit(EXIT_FAILURE);
    }

    struct addrinfo *res = *res_out;
    int fd = socket(res->ai_family, res->ai_socktype | SOCK_CLOEXEC,
                    res->ai_protocol);
    if (fd < 0) {
        die("socket");
    }

    /* 1 秒接收超时，超时后 recvfrom 返回 -1，errno 为 EAGAIN。 */
    struct timeval tv = {1, 0};
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) < 0) {
        die("setsockopt");
    }
    return fd;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s HOST PORT ROLL\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct addrinfo *res = NULL;
    int fd = udp_client_socket(argv[1], argv[2], &res);

    const char *request = argv[3];
    char reply[256];

    for (int attempt = 1; attempt <= 3; attempt++) {
        if (sendto(fd, request, strlen(request), 0, res->ai_addr,
                   res->ai_addrlen) < 0) {
            die("sendto");
        }

        ssize_t n = recvfrom(fd, reply, sizeof reply - 1, 0, NULL, NULL);
        if (n >= 0) {
            reply[n] = '\0';
            printf("attempt %d: %s", attempt, reply);
            freeaddrinfo(res);
            close(fd);
            return EXIT_SUCCESS;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("attempt %d: timeout, retrying\n", attempt);
            continue;
        }
        die("recvfrom");
    }

    puts("giving up after 3 attempts");
    freeaddrinfo(res);
    close(fd);
    return EXIT_FAILURE;
}