#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void sleep_ms(long ms) {
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s HOST PORT [nosignal]\n", argv[0]);
        return EXIT_FAILURE;
    }
    int use_nosignal = argc > 3 && strcmp(argv[3], "nosignal") == 0;

    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        die("socket");
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) != 1) {
        fprintf(stderr, "bad address: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        die("connect");
    }

    char buf[256];
    memset(buf, 'a', sizeof buf);
    int flags = use_nosignal ? MSG_NOSIGNAL : 0;

    for (int i = 1; i <= 3; i++) {
        ssize_t n = send(fd, buf, sizeof buf, flags);
        if (n < 0) {
            printf("send #%d: %s\n", i, strerror(errno));
            break;
        }
        printf("send #%d: wrote %zd bytes\n", i, n);
        fflush(stdout);
        sleep_ms(100);
    }
    close(fd);
    return EXIT_SUCCESS;
}