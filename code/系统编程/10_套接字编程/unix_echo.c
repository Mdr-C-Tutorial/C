#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SUN_PATH_MAX (sizeof(((struct sockaddr_un *)0)->sun_path))

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static int write_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    size_t left = len;

    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        p += n;
        left -= (size_t)n;
    }
    return 0;
}

static void fill_addr(struct sockaddr_un *addr, const char *path) {
    memset(addr, 0, sizeof *addr);
    addr->sun_family = AF_UNIX;
    snprintf(addr->sun_path, SUN_PATH_MAX, "%s", path);
}

/* 服务器：接受一个连接，打印对端凭证，把收到的行转成大写发回。 */
static void server(const char *path) {
    int lfd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (lfd < 0) {
        die("socket");
    }

    struct sockaddr_un addr;
    fill_addr(&addr, path);
    if (bind(lfd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        die("bind");
    }
    if (listen(lfd, 4) < 0) {
        die("listen");
    }

    int cfd = accept(lfd, NULL, NULL);
    if (cfd < 0) {
        die("accept");
    }

    /* Linux 专有：SO_PEERCRED 给出对端进程的 PID、UID 和 GID。 */
    struct ucred cred;
    socklen_t cred_len = sizeof cred;
    if (getsockopt(cfd, SOL_SOCKET, SO_PEERCRED, &cred, &cred_len) < 0) {
        die("getsockopt(SO_PEERCRED)");
    }
    printf("server: peer pid=%d uid=%u gid=%u\n", cred.pid,
           (unsigned)cred.uid, (unsigned)cred.gid);
    fflush(stdout);

    char buf[256];
    ssize_t n = recv(cfd, buf, sizeof buf - 1, 0);
    if (n < 0) {
        die("recv");
    }
    buf[n] = '\0';
    char *nl = strchr(buf, '\n');
    if (nl != NULL) {
        *nl = '\0';
    }
    printf("server: received \"%s\"\n", buf);

    for (char *q = buf; *q != '\0'; q++) {
        if (*q >= 'a' && *q <= 'z') {
            *q = (char)(*q - 'a' + 'A');
        }
    }
    if (write_all(cfd, buf, strlen(buf)) < 0) {
        die("write");
    }

    close(cfd);
    close(lfd);
}

/* 客户端在子进程里跑；服务器可能还没 bind 好，所以连不上就重试一会儿。 */
static void client(const char *path) {
    for (int i = 0; i < 200; i++) {
        int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            _exit(EXIT_FAILURE);
        }
        struct sockaddr_un addr;
        fill_addr(&addr, path);
        if (connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0) {
            const char *msg = "hello over unix socket\n";
            if (write_all(fd, msg, strlen(msg)) < 0) {
                _exit(EXIT_FAILURE);
            }
            char buf[256];
            ssize_t n = recv(fd, buf, sizeof buf - 1, 0);
            if (n > 0) {
                buf[n] = '\0';
                printf("client: reply \"%s\"\n", buf);
                fflush(stdout);
            }
            close(fd);
            _exit(EXIT_SUCCESS);
        }
        close(fd);
        struct timespec ts = {0, 10 * 1000 * 1000L};
        nanosleep(&ts, NULL);
    }
    _exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    const char *path = argc > 1 ? argv[1] : "echo.sock";

    /* bind 不会覆盖已有的路径，先删掉上次运行留下的套接字文件。 */
    unlink(path);

    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) {
        die("fork");
    }
    if (pid == 0) {
        client(path);
    }

    server(path);
    waitpid(pid, NULL, 0);
    unlink(path);
    return EXIT_SUCCESS;
}