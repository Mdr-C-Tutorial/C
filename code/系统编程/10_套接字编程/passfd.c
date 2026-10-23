#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 在已连接的 Unix 域套接字上，把已打开的文件描述符发给对端。 */
static int send_fd(int sock, int fd) {
    struct msghdr msg = {0};
    char byte = 'F';
    struct iovec iov = {&byte, 1};

    char control[CMSG_SPACE(sizeof(int))];
    memset(control, 0, sizeof control);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof control;

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));

    return sendmsg(sock, &msg, 0) < 0 ? -1 : 0;
}

/* 从对端接收一个文件描述符。 */
static int recv_fd(int sock) {
    struct msghdr msg = {0};
    char byte = 0;
    struct iovec iov = {&byte, 1};

    char control[CMSG_SPACE(sizeof(int))];
    memset(control, 0, sizeof control);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof control;

    if (recvmsg(sock, &msg, 0) <= 0) {
        return -1;
    }

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == NULL || cmsg->cmsg_level != SOL_SOCKET ||
        cmsg->cmsg_type != SCM_RIGHTS) {
        return -1;
    }

    int fd = -1;
    memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
    return fd;
}

int main(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) < 0) {
        die("socketpair");
    }

    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) {
        die("fork");
    }
    if (pid == 0) {
        close(sv[0]);

        /* 打开当前目录，把描述符整个交给父进程。 */
        int dirfd = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dirfd < 0) {
            die("open");
        }
        if (send_fd(sv[1], dirfd) < 0) {
            die("send_fd");
        }
        close(dirfd);
        close(sv[1]);
        _exit(EXIT_SUCCESS);
    }

    close(sv[1]);
    int dirfd = recv_fd(sv[0]);
    if (dirfd < 0) {
        die("recv_fd");
    }
    printf("parent received fd %d\n", dirfd);

    /* 用收到的描述符做 openat，访问当前目录里的文件。 */
    int fd = openat(dirfd, "boundary.c", O_RDONLY);
    if (fd < 0) {
        die("openat");
    }
    char buf[64];
    ssize_t n = read(fd, buf, sizeof buf - 1);
    if (n < 0) {
        die("read");
    }
    buf[n] = '\0';
    printf("first line via received fd: %.*s\n", (int)strcspn(buf, "\n"),
           buf);

    close(fd);
    close(dirfd);
    close(sv[0]);
    waitpid(pid, NULL, 0);
    return EXIT_SUCCESS;
}