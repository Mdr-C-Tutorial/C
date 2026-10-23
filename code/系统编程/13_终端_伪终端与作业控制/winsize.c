#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static volatile sig_atomic_t resized;

static void on_winch(int sig) {
    (void)sig;
    resized = 1;
}

static void print_size(const char *label) {
    struct winsize ws;

    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) {
        perror("TIOCGWINSZ");
        exit(EXIT_FAILURE);
    }
    printf("%s: %u rows x %u columns (%u x %u pixels)\n", label, ws.ws_row, ws.ws_col,
           ws.ws_xpixel, ws.ws_ypixel);
}

int main(void) {
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_winch;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGWINCH, &sa, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    print_size("initial");
    printf("resize the window; ctrl-c to quit\n");
    fflush(stdout);

    while (!resized) {
        pause();
    }
    print_size("after SIGWINCH");
    return 0;
}