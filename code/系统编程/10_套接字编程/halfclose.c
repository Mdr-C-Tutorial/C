#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
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

/* 写到所有字节都送出或出错为止。 */
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

/* 服务器：一直读到文件结束（客户端半关闭），把整段输入转成大写后发回。 */
static void server(int lfd) {
    int cfd = accept(lfd, NULL, NULL);
    if (cfd < 0) {
        die("accept");
    }

    char buf[4096];
    size_t used = 0;
    for (;;) {
        ssize_t n = recv(cfd, buf + used, sizeof buf - used - 1, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            die("recv");
        }
        if (n == 0) {
            break;
        }
        used += (size_t)n;
    }
    buf[used] = '\0';
    printf("server: got %zu bytes before EOF\n", used);
    fflush(stdout);

    for (size_t i = 0; i < used; i++) {
        buf[i] = (char)toupper((unsigned char)buf[i]);
    }
    if (write_all(cfd, buf, used) < 0) {
        die("write");
    }
    close(cfd);
}

/* 客户端：发完请求就半关闭，再继续读应答直到对端关闭。 */
static void client(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket");
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        die("connect");
    }

    const char *request = "hello server\n";
    if (write_all(fd, request, strlen(request)) < 0) {
        die("write");
    }
    puts("client: request sent, calling shutdown(SHUT_WR)");
    fflush(stdout);
    if (shutdown(fd, SHUT_WR) < 0) {
        die("shutdown");
    }

    char buf[4096];
    for (;;) {
        ssize_t n = recv(fd, buf, sizeof buf, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            die("recv");
        }
        if (n == 0) {
            puts("client: recv returned 0");
            fflush(stdout);
            break;
        }
        printf("client received: %.*s", (int)n, buf);
        fflush(stdout);
    }
    close(fd);
}

int main(void) {
    int lfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (lfd < 0) {
        die("socket");
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(lfd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        die("bind");
    }
    if (listen(lfd, 1) < 0) {
        die("listen");
    }
    socklen_t len = sizeof addr;
    if (getsockname(lfd, (struct sockaddr *)&addr, &len) < 0) {
        die("getsockname");
    }

    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) {
        die("fork");
    }
    if (pid == 0) {
        close(lfd);
        client(ntohs(addr.sin_port));
        exit(EXIT_SUCCESS);
    }

    server(lfd);
    close(lfd);
    waitpid(pid, NULL, 0);
    return EXIT_SUCCESS;
}