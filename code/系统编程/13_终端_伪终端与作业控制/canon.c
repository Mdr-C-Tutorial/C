#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static struct termios original;

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void show(const char *label, ssize_t n, const char *buf) {
    if (n < 0) {
        printf("%s: read returned -1, errno = %d (%s)\n", label, errno, strerror(errno));
        return;
    }
    printf("%s: read returned %zd byte(s): \"", label, n);
    for (ssize_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)buf[i];

        if (c == '\n') {
            printf("\\n");
        } else if (c == '\r') {
            printf("\\r");
        } else if (c == '\t') {
            printf("\\t");
        } else if (c < 0x20 || c == 0x7f) {
            printf("\\x%02x", c);
        } else {
            putchar(c);
        }
    }
    printf("\"\n");
}

static void set_canonical(int on) {
    struct termios t = original;

    if (on) {
        t.c_lflag |= ICANON | ECHO;
    } else {
        t.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;
    }
    if (tcsetattr(STDIN_FILENO, TCSANOW, &t) == -1) {
        die("tcsetattr");
    }
}

int main(void) {
    char buf[64];

    if (tcgetattr(STDIN_FILENO, &original) == -1) {
        die("tcgetattr");
    }
    printf("Type two lines, each under 60 characters.\n");

    set_canonical(1);
    printf("--- canonical mode (ICANON set) ---\n");
    printf("first read waiting...\n");
    fflush(stdout);
    ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
    show("first ", n, buf);

    printf("second read waiting...\n");
    fflush(stdout);
    n = read(STDIN_FILENO, buf, sizeof buf);
    show("second", n, buf);

    set_canonical(0);
    printf("--- noncanonical mode (ICANON and ECHO cleared, VMIN=1, VTIME=0) ---\n");
    printf("press three keys\n");
    fflush(stdout);
    for (int i = 0; i < 3; i++) {
        n = read(STDIN_FILENO, buf, sizeof buf);
        show("key  ", n, buf);
    }

    set_canonical(1);
    printf("--- back to canonical mode ---\n");
    return 0;
}