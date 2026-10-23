#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void set_vmin_vtime(int vmin, int vtime) {
    struct termios t;

    if (tcgetattr(STDIN_FILENO, &t) == -1) {
        die("tcgetattr");
    }
    t.c_lflag &= ~(tcflag_t)ICANON;
    t.c_lflag &= ~(tcflag_t)ECHO;
    t.c_cc[VMIN] = (cc_t)vmin;
    t.c_cc[VTIME] = (cc_t)vtime;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &t) == -1) {
        die("tcsetattr");
    }
}

static double now(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        die("clock_gettime");
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s VMIN VTIME\n", argv[0]);
        return EXIT_FAILURE;
    }
    int vmin = atoi(argv[1]);
    int vtime = atoi(argv[2]);

    set_vmin_vtime(vmin, vtime);
    printf("VMIN=%d VTIME=%d: three reads follow\r\n", vmin, vtime);
    fflush(stdout);

    char buf[64];
    for (int i = 0; i < 3; i++) {
        double t0 = now();
        ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
        double dt = now() - t0;

        if (n == -1) {
            printf("read %d: -1 after %.2f s (errno %d)\r\n", i + 1, dt, errno);
        } else {
            printf("read %d: %zd byte(s) after %.2f s\r\n", i + 1, n, dt);
        }
        fflush(stdout);
    }
    return 0;
}