/* wsapoll_server.c —— 用 WSAPoll 单线程服务多个客户端的行回声服务器。
 *
 * gcc -std=c17 -Wall -Wextra wsapoll_server.c -o wsapoll_server.exe -lws2_32
 * 用法: wsapoll_server [端口]，默认 23462，只监听 127.0.0.1。
 * 客户端断开后服务器继续等待；空闲 5 秒没有连接时退出。
 */

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#define MAX_CONNS 8
#define LINE_CAP 1024

struct conn {
    SOCKET fd;
    int id;
    char buf[LINE_CAP];
    int used;
};

static void die(const char *what) {
    fprintf(stderr, "%s: error %d\n", what, WSAGetLastError());
    exit(EXIT_FAILURE);
}

static SOCKET tcp_listen(const char *port) {
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo("127.0.0.1", port, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        exit(EXIT_FAILURE);
    }

    SOCKET fd = INVALID_SOCKET;
    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == INVALID_SOCKET) {
            continue;
        }
        BOOL exclusive = TRUE;
        if (setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                       (const char *)&exclusive, sizeof exclusive) ==
            SOCKET_ERROR) {
            closesocket(fd);
            fd = INVALID_SOCKET;
            continue;
        }
        if (bind(fd, p->ai_addr, (int)p->ai_addrlen) == 0) {
            break;
        }
        closesocket(fd);
        fd = INVALID_SOCKET;
    }
    freeaddrinfo(res);

    if (fd == INVALID_SOCKET) {
        die("bind");
    }
    u_long nonblocking = 1;
    if (ioctlsocket(fd, FIONBIO, &nonblocking) == SOCKET_ERROR) {
        die("ioctlsocket");
    }
    if (listen(fd, 16) == SOCKET_ERROR) {
        die("listen");
    }
    return fd;
}

/* 把一行大写后发回去，返回 0 表示连接该关了。 */
static int handle_line(struct conn *c, const char *line, int len) {
    char out[LINE_CAP];
    if (len > LINE_CAP - 2) {
        return 0;
    }
    for (int i = 0; i < len; i++) {
        char ch = line[i];
        out[i] = (ch >= 'a' && ch <= 'z') ? (char)(ch - 'a' + 'A') : ch;
    }
    out[len] = '\n';
    int total = len + 1;
    if (len > 0 && out[len - 1] == '\r') {
        out[len - 1] = '\n';
        total = len;
    }
    if (total == 5 + 1 && memcmp(out, "QUIT\r\n", 6) == 0) {
        return 0;
    }
    if (total == 4 + 1 && memcmp(out, "QUIT\n", 5) == 0) {
        return 0;
    }
    int sent = 0;
    while (sent < total) {
        int n = send(c->fd, out + sent, total - sent, 0);
        if (n == SOCKET_ERROR) {
            return 0;
        }
        sent += n;
    }
    return 1;
}

/* 从缓冲区里切出完整的行并处理，返回 0 表示连接该关了。 */
static int consume_lines(struct conn *c) {
    int start = 0;
    for (int i = 0; i < c->used; i++) {
        if (c->buf[i] != '\n') {
            continue;
        }
        if (!handle_line(c, c->buf + start, i - start)) {
            return 0;
        }
        start = i + 1;
    }
    memmove(c->buf, c->buf + start, (size_t)(c->used - start));
    c->used -= start;
    return 1;
}

int main(int argc, char **argv) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return EXIT_FAILURE;
    }

    const char *port = argc > 1 ? argv[1] : "23462";
    SOCKET listener = tcp_listen(port);
    printf("listening on 127.0.0.1:%s\n", port);
    fflush(stdout);

    struct conn conns[MAX_CONNS];
    int nconns = 0;
    int idle_rounds = 0;
    int served = 0;
    int next_id = 0;

    for (;;) {
        WSAPOLLFD pfds[MAX_CONNS + 1];
        pfds[0].fd = listener;
        pfds[0].events = POLLRDNORM;
        pfds[0].revents = 0;
        for (int i = 0; i < nconns; i++) {
            pfds[i + 1].fd = conns[i].fd;
            pfds[i + 1].events = POLLRDNORM;
            pfds[i + 1].revents = 0;
        }

        int ready = WSAPoll(pfds, (ULONG)(nconns + 1), 1000);
        if (ready == SOCKET_ERROR) {
            die("WSAPoll");
        }
        if (ready == 0) {
            if (served > 0 && ++idle_rounds >= 5) {
                printf("idle for 5 s, exiting\n");
                fflush(stdout);
                break;
            }
            continue;
        }
        idle_rounds = 0;

        /* 先处理已有连接，按下标从后往前删，避免移动数组。 */
        for (int i = nconns - 1; i >= 0; i--) {
            if (pfds[i + 1].revents == 0) {
                continue;
            }
            int closed = 0;
            if (pfds[i + 1].revents & POLLNVAL) {
                closed = 1;
            }
            while (!closed) {
                int room = LINE_CAP - conns[i].used;
                int n = recv(conns[i].fd, conns[i].buf + conns[i].used, room, 0);
                if (n == 0) {
                    closed = 1;
                    break;
                }
                if (n == SOCKET_ERROR) {
                    int err = WSAGetLastError();
                    if (err == WSAEWOULDBLOCK) {
                        break;
                    }
                    closed = 1;
                    break;
                }
                conns[i].used += n;
                if (!consume_lines(&conns[i])) {
                    closed = 1;
                    break;
                }
            }
            if (closed) {
                printf("connection %d closed\n", conns[i].id);
                fflush(stdout);
                closesocket(conns[i].fd);
                conns[i] = conns[nconns - 1];
                nconns--;
            }
        }

        /* 监听套接字可读：把待接受的连接全部取走。 */
        if (pfds[0].revents & POLLRDNORM) {
            for (;;) {
                SOCKET c = accept(listener, NULL, NULL);
                if (c == INVALID_SOCKET) {
                    break;
                }
                if (nconns == MAX_CONNS) {
                    closesocket(c);
                    continue;
                }
                u_long nonblocking = 1;
                ioctlsocket(c, FIONBIO, &nonblocking);
                conns[nconns].fd = c;
                conns[nconns].id = ++next_id;
                conns[nconns].used = 0;
                nconns++;
                served++;
                printf("client %d connected\n", next_id);
                fflush(stdout);
            }
        }
    }

    for (int i = 0; i < nconns; i++) {
        closesocket(conns[i].fd);
    }
    closesocket(listener);
    WSACleanup();
    return 0;
}