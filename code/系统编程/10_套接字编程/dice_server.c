#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 解析 "3d6"：返回骰子个数，*sides 为面数。格式不对返回 -1。 */
static int parse_roll(const char *text, int *sides) {
    char *end = NULL;
    long count = strtol(text, &end, 10);
    if (end == text || (*end != 'd' && *end != 'D')) {
        return -1;
    }
    char *end2 = NULL;
    long faces = strtol(end + 1, &end2, 10);
    if (end2 == end + 1 || *end2 != '\0' || count < 1 || count > 100 ||
        faces < 2 || faces > 1000) {
        return -1;
    }
    *sides = (int)faces;
    return (int)count;
}

static int udp_bind(const char *host, const char *port) {
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo(host, port, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        exit(EXIT_FAILURE);
    }

    int fd = -1;
    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype | SOCK_CLOEXEC,
                    p->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (bind(fd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd < 0) {
        die("bind");
    }
    return fd;
}

int main(int argc, char *argv[]) {
    const char *port = argc > 1 ? argv[1] : "21050";
    int drop_percent = argc > 2 ? atoi(argv[2]) : 0;

    int fd = udp_bind("127.0.0.1", port);
    printf("dice server on 127.0.0.1 port %s", port);
    if (drop_percent > 0) {
        printf(", dropping %d%% of requests", drop_percent);
    }
    putchar('\n');
    fflush(stdout);

    srand((unsigned)time(NULL));

    for (;;) {
        char line[128];
        struct sockaddr_storage peer;
        socklen_t peer_len = sizeof peer;

        ssize_t n = recvfrom(fd, line, sizeof line - 1, 0,
                             (struct sockaddr *)&peer, &peer_len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            die("recvfrom");
        }
        line[n] = '\0';

        if (drop_percent > 0 && rand() % 100 < drop_percent) {
            printf("dropped request \"%s\"\n", line);
            fflush(stdout);
            continue;
        }

        char *nl = strchr(line, '\n');
        if (nl != NULL) {
            *nl = '\0';
        }

        int sides = 0;
        int count = parse_roll(line, &sides);
        char reply[256];
        int len;

        if (count < 0) {
            len = snprintf(reply, sizeof reply, "error: expected NdM\n");
        } else {
            int total = 0;
            len = snprintf(reply, sizeof reply, "%s =", line);
            for (int i = 0; i < count; i++) {
                int v = rand() % sides + 1;
                total += v;
                len += snprintf(reply + len, sizeof reply - (size_t)len,
                                " %d", v);
            }
            len += snprintf(reply + len, sizeof reply - (size_t)len,
                            " (total %d)\n", total);
        }

        if (sendto(fd, reply, (size_t)len, 0, (struct sockaddr *)&peer,
                   peer_len) < 0) {
            die("sendto");
        }
    }
}