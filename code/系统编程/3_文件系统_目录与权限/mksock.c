#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s PATH\n", argv[0]);
        return EXIT_FAILURE;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd == -1) {
        die("socket");
    }

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    if (strlen(argv[1]) >= sizeof addr.sun_path) {
        fprintf(stderr, "%s: path too long\n", argv[1]);
        return EXIT_FAILURE;
    }
    strcpy(addr.sun_path, argv[1]);
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) == -1) {
        die("bind");
    }
    printf("bound AF_UNIX socket to %s\n", argv[1]);
    if (close(fd) == -1) {
        die("close");
    }
    return 0;
}
