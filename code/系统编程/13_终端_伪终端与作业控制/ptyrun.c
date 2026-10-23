#define _XOPEN_SOURCE 700

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static int write_all(int fd, const char *buf, size_t len) {
    size_t off = 0;

    while (off < len) {
        ssize_t n = write(fd, buf + off, len - off);

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EIO || errno == EPIPE) {
                return -1;
            }
            die("write");
        }
        off += (size_t)n;
    }
    return 0;
}

static long now_ms(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        die("clock_gettime");
    }
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static void set_size(int fd, int rows, int cols) {
    struct winsize ws = {
        .ws_row = (unsigned short)rows,
        .ws_col = (unsigned short)cols,
    };

    if (ioctl(fd, TIOCSWINSZ, &ws) == -1) {
        die("TIOCSWINSZ");
    }
}

int main(int argc, char *argv[]) {
    int rows = 24;
    int cols = 80;
    int change_rows = 0;
    int change_cols = 0;
    long change_ms = -1;
    int quiet = 0;
    int opt;

    while ((opt = getopt(argc, argv, "+qs:c:")) != -1) {
        switch (opt) {
        case 'q':
            quiet = 1;
            break;
        case 's':
            if (sscanf(optarg, "%dx%d", &rows, &cols) != 2) {
                fprintf(stderr, "bad size: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'c':
            if (sscanf(optarg, "%dx%d@%ld", &change_rows, &change_cols, &change_ms) != 3) {
                fprintf(stderr, "bad resize: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        default:
            fprintf(stderr, "usage: %s [-q] [-s ROWSxCOLS] [-c ROWSxCOLS@MS] command...\n",
                    argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (optind >= argc) {
        fprintf(stderr, "usage: %s [-q] [-s ROWSxCOLS] [-c ROWSxCOLS@MS] command...\n", argv[0]);
        return EXIT_FAILURE;
    }

    int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master == -1) {
        die("posix_openpt");
    }
    if (grantpt(master) == -1) {
        die("grantpt");
    }
    if (unlockpt(master) == -1) {
        die("unlockpt");
    }
    char *slave_name = ptsname(master);
    if (slave_name == NULL) {
        die("ptsname");
    }

    set_size(master, rows, cols);

    int ready[2];
    if (pipe(ready) == -1) {
        die("pipe");
    }

    pid_t pid = fork();
    if (pid == -1) {
        die("fork");
    }
    if (pid == 0) {
        close(ready[0]);
        if (setsid() == -1) {
            die("setsid");
        }
        int slave = open(slave_name, O_RDWR);
        if (slave == -1) {
            die("open slave");
        }
        if (dup2(slave, STDIN_FILENO) == -1 || dup2(slave, STDOUT_FILENO) == -1 ||
            dup2(slave, STDERR_FILENO) == -1) {
            die("dup2");
        }
        if (slave > STDERR_FILENO) {
            close(slave);
        }
        close(master);
        char one = '1';
        if (write(ready[1], &one, 1) != 1) {
            _exit(127);
        }
        close(ready[1]);
        execvp(argv[optind], &argv[optind]);
        perror("execvp");
        _exit(127);
    }

    close(ready[1]);
    char ack;
    while (read(ready[0], &ack, 1) == -1 && errno == EINTR) {
    }
    close(ready[0]);

    long change_at = -1;
    if (change_ms >= 0) {
        change_at = now_ms() + change_ms;
    }

    struct pollfd fds[2] = {
        {.fd = STDIN_FILENO, .events = POLLIN},
        {.fd = master, .events = POLLIN},
    };
    char buf[4096];
    int stdin_open = 1;

    for (;;) {
        if (change_at >= 0 && now_ms() >= change_at) {
            set_size(master, change_rows, change_cols);
            change_at = -1;
        }
        int timeout = -1;
        if (change_at >= 0) {
            long remain = change_at - now_ms();
            timeout = remain > 0 ? (int)remain : 0;
        }
        fds[0].fd = stdin_open ? STDIN_FILENO : -1;

        int ready = poll(fds, 2, timeout);
        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }
            die("poll");
        }
        if (ready == 0) {
            continue;
        }

        if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            ssize_t n = read(STDIN_FILENO, buf, sizeof buf);

            if (n > 0) {
                if (write_all(master, buf, (size_t)n) == -1) {
                    break;
                }
            } else if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
                stdin_open = 0;
            }
        }
        if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            ssize_t n = read(master, buf, sizeof buf);

            if (n > 0) {
                if (write_all(STDOUT_FILENO, buf, (size_t)n) == -1) {
                    break;
                }
            } else if (n == 0 || (errno == EIO && n == -1)) {
                break;
            } else if (errno != EINTR && errno != EAGAIN) {
                die("read master");
            }
        }
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        die("waitpid");
    }
    if (!quiet) {
        if (WIFEXITED(status)) {
            fprintf(stderr, "[ptyrun] child exited with status %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "[ptyrun] child killed by signal %d (%s)\n", WTERMSIG(status),
                    strsignal(WTERMSIG(status)));
        }
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return EXIT_FAILURE;
}