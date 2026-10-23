#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

int main(void) {
    struct termios t;

    if (tcgetattr(STDIN_FILENO, &t) == -1) {
        perror("tcgetattr");
        return EXIT_FAILURE;
    }
    t.c_lflag &= ~(tcflag_t)ECHO;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &t) == -1) {
        perror("tcsetattr");
        return EXIT_FAILURE;
    }
    printf("echo is now off; this program exits without restoring it\n");
    fflush(stdout);
    return 0;
}