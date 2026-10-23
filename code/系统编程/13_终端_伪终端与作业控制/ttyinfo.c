#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

int main(void) {
    for (int fd = 0; fd <= 2; fd++) {
        const char *name = isatty(fd) ? ttyname(fd) : "(not a terminal)";

        printf("fd %d: %s\n", fd, name ? name : "(unknown)");
    }

    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "stdin is not a terminal; run inside a terminal or ptyrun\n");
        return EXIT_FAILURE;
    }

    struct termios t;
    if (tcgetattr(STDIN_FILENO, &t) == -1) {
        perror("tcgetattr");
        return EXIT_FAILURE;
    }

    printf("c_iflag = 0x%08lx  (ICRNL=%d IXON=%d)\n", (unsigned long)t.c_iflag,
           (t.c_iflag & ICRNL) != 0, (t.c_iflag & IXON) != 0);
    printf("c_oflag = 0x%08lx  (OPOST=%d ONLCR=%d)\n", (unsigned long)t.c_oflag,
           (t.c_oflag & OPOST) != 0, (t.c_oflag & ONLCR) != 0);
    printf("c_cflag = 0x%08lx  (CS8=%d CREAD=%d)\n", (unsigned long)t.c_cflag,
           (t.c_cflag & CS8) == CS8, (t.c_cflag & CREAD) != 0);
    printf("c_lflag = 0x%08lx  (ICANON=%d ECHO=%d ISIG=%d)\n", (unsigned long)t.c_lflag,
           (t.c_lflag & ICANON) != 0, (t.c_lflag & ECHO) != 0, (t.c_lflag & ISIG) != 0);
    printf("VMIN = %d  VTIME = %d\n", t.c_cc[VMIN], t.c_cc[VTIME]);
    printf("VERASE = 0x%02x  VINTR = 0x%02x  VEOF = 0x%02x\n", t.c_cc[VERASE], t.c_cc[VINTR],
           t.c_cc[VEOF]);
    return 0;
}