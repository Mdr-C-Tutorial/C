#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 把 revents 里的标志转成文字追加到 buf。 */
static void describe(short revents, char *buf) {
    buf[0] = '\0';
    if (revents == 0) {
        strcpy(buf, "0");
        return;
    }
    if (revents & POLLIN) {
        strcat(buf, " POLLIN");
    }
    if (revents & POLLOUT) {
        strcat(buf, " POLLOUT");
    }
    if (revents & POLLHUP) {
        strcat(buf, " POLLHUP");
    }
    if (revents & POLLERR) {
        strcat(buf, " POLLERR");
    }
    if (revents & POLLNVAL) {
        strcat(buf, " POLLNVAL");
    }
}

static void report(const char *when, struct pollfd *pfd, int n) {
    char buf[64];
    printf("%s：", when);
    for (int i = 0; i < n; i++) {
        describe(pfd[i].revents, buf);
        printf(" fd%d 的 revents =%s；", pfd[i].fd, buf);
    }
    putchar('\n');
}

int main(void) {
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        die("signal");
    }

    int p[2];
    if (pipe(p) == -1) {
        die("pipe");
    }
    if (fcntl(p[0], F_SETFL, O_NONBLOCK) == -1 ||
        fcntl(p[1], F_SETFL, O_NONBLOCK) == -1) {
        die("fcntl");
    }
    struct pollfd pfd[2] = {
        {.fd = p[0], .events = POLLIN},
        {.fd = p[1], .events = POLLOUT},
    };

    if (poll(pfd, 2, 0) == -1) {
        die("poll");
    }
    report("空管道", pfd, 2);

    if (write(p[1], "x", 1) != 1) {
        die("write");
    }
    if (poll(pfd, 2, 0) == -1) {
        die("poll");
    }
    report("写入 1 字节", pfd, 2);

    char ch;
    if (read(p[0], &ch, 1) != 1) {
        die("read");
    }
    if (poll(pfd, 2, 0) == -1) {
        die("poll");
    }
    report("读空之后", pfd, 2);

    if (close(p[1]) == -1) {
        die("close");
    }
    if (poll(pfd, 1, 0) == -1) {
        die("poll");
    }
    report("关闭写端", pfd, 1);
    ssize_t n = read(p[0], &ch, 1);
    printf("关闭写端后 read 返回 %zd\n\n", n);

    int q[2];
    if (pipe(q) == -1) {
        die("pipe");
    }
    struct pollfd qfd = {.fd = q[1], .events = POLLOUT};
    if (close(q[0]) == -1) {
        die("close");
    }
    if (poll(&qfd, 1, 0) == -1) {
        die("poll");
    }
    report("关闭读端", &qfd, 1);
    errno = 0;
    n = write(q[1], "x", 1);
    printf("关闭读端后 write 返回 %zd，errno = %d（%s）\n", n, errno,
           strerror(errno));
    close(q[1]);
    close(p[0]);
    return 0;
}