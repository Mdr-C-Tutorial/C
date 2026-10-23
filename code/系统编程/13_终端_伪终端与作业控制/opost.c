#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int on = argc > 1 && argv[1][0] == '1';
    struct termios t;

    if (tcgetattr(STDOUT_FILENO, &t) == -1) {
        perror("tcgetattr");
        return EXIT_FAILURE;
    }
    if (on) {
        t.c_oflag |= OPOST | ONLCR;
    } else {
        t.c_oflag &= ~(tcflag_t)OPOST;
    }
    if (tcsetattr(STDOUT_FILENO, TCSANOW, &t) == -1) {
        perror("tcsetattr");
        return EXIT_FAILURE;
    }
    printf("one\ntwo\nthree\n");
    return 0;
}