#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static struct termios saved;
static volatile sig_atomic_t got_signal;

static void on_signal(int sig) {
    got_signal = sig;
}

static void restore(void) {
    if (tcsetattr(STDIN_FILENO, TCSANOW, &saved) == -1) {
        perror("tcsetattr restore");
    }
}

static void die(const char *what) {
    restore();
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    if (tcgetattr(STDIN_FILENO, &saved) == -1) {
        perror("tcgetattr");
        return EXIT_FAILURE;
    }
    atexit(restore);

    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    struct termios t = saved;

    t.c_lflag &= ~(tcflag_t)ECHO;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &t) == -1) {
        die("tcsetattr");
    }

    char pass[64];
    printf("Password: ");
    fflush(stdout);

    ssize_t n;
    do {
        n = read(STDIN_FILENO, pass, sizeof pass - 1);
    } while (n == -1 && errno == EINTR && got_signal == 0);

    printf("\n");
    restore();

    if (got_signal != 0) {
        printf("interrupted by signal %d (%s), terminal restored\n", (int)got_signal,
               strsignal((int)got_signal));
        return 128 + (int)got_signal;
    }
    if (n <= 0) {
        printf("no password read\n");
        return EXIT_FAILURE;
    }
    pass[n] = '\0';
    size_t len = strlen(pass);

    if (len > 0 && pass[len - 1] == '\n') {
        pass[len - 1] = '\0';
    }
    printf("read %zu character(s) without echo\n", strlen(pass));
    return 0;
}