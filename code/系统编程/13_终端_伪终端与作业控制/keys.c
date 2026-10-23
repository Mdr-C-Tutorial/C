#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static struct termios saved;

static void restore(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &saved);
}

static void die(const char *what) {
    restore();
    perror(what);
    exit(EXIT_FAILURE);
}

static void enable_raw(void) {
    struct termios t;

    if (tcgetattr(STDIN_FILENO, &t) == -1) {
        die("tcgetattr");
    }
    saved = t;

    t.c_iflag &= ~(tcflag_t)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    t.c_oflag &= ~(tcflag_t)OPOST;
    t.c_cflag |= CS8;
    t.c_lflag &= ~(tcflag_t)(ECHO | ICANON | IEXTEN | ISIG);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &t) == -1) {
        die("tcsetattr");
    }
}

static const char *describe(unsigned char c) {
    static char buf[32];

    switch (c) {
    case 0x1b:
        return "ESC";
    case 0x03:
        return "Ctrl-C (ISIG off: arrives as a byte)";
    case 0x04:
        return "Ctrl-D";
    case 0x7f:
        return "DEL (Backspace)";
    case '\r':
        return "CR";
    case '\n':
        return "LF";
    case '\t':
        return "TAB";
    default:
        break;
    }
    if (c >= 0x20 && c < 0x7f) {
        snprintf(buf, sizeof buf, "'%c'", c);
        return buf;
    }
    if (c < 0x20) {
        snprintf(buf, sizeof buf, "Ctrl-%c", 'A' + c - 1);
        return buf;
    }
    snprintf(buf, sizeof buf, "byte 0x%02x", c);
    return buf;
}

int main(void) {
    enable_raw();
    atexit(restore);

    printf("Press keys; q quits. Bytes are shown in hex.\r\n");
    fflush(stdout);

    for (;;) {
        unsigned char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            die("read");
        }
        if (n == 0) {
            continue;
        }
        printf("0x%02x  %-40s\r\n", c, describe(c));
        fflush(stdout);
        if (c == 'q') {
            break;
        }
    }
    return 0;
}