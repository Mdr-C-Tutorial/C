#define _POSIX_C_SOURCE 200809L

#include <signal.h>
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
    t.c_lflag &= ~(tcflag_t)ECHO;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &t) == -1) {
        perror("tcsetattr");
        return EXIT_FAILURE;
    }
    printf("echo off, stopping myself with SIGTSTP\n");
    fflush(stdout);
    if (raise(SIGTSTP) != 0) {
        perror("raise");
    }
    if (tcgetattr(STDIN_FILENO, &t) == -1) {
        perror("tcgetattr");
        return EXIT_FAILURE;
    }
    printf("resumed; ECHO is %s\n", (t.c_lflag & ECHO) != 0 ? "on" : "off");
    return 0;
}