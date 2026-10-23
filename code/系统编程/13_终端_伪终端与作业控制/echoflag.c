#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

int main(void) {
    struct termios t;

    if (tcgetattr(STDIN_FILENO, &t) == -1) {
        perror("tcgetattr");
        return EXIT_FAILURE;
    }
    printf("ECHO is %s\n", (t.c_lflag & ECHO) != 0 ? "on" : "off");
    return 0;
}