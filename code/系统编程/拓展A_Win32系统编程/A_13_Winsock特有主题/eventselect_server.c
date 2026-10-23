/* eventselect_server.c —— 用 WSAEventSelect 把套接字变成可等待对象。
 *
 * gcc -std=c17 -Wall -Wextra eventselect_server.c -o eventselect_server.exe -lws2_32
 * 用法: eventselect_server [端口] [运行秒数]，默认 23467，3 秒后停止事件置位并退出。
 */

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#define MAX_CONNS 16
#define LINE_CAP 1024

struct conn {
    SOCKET fd;
    WSAEVENT ev;
    char buf[LINE_CAP];
    int used;
    int id;
};

/* 停止线程：等指定毫秒数后把停止事件置为有信号。 */
static DWORD WINAPI stopper(LPVOID arg) {
    HANDLE stop = (HANDLE)arg;
    Sleep(3000);
    SetEvent(stop);
    printf("[stopper] stop event signaled\n");
    fflush(stdout);
    return 0;
}

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
        setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   (const char *)&exclusive, sizeof exclusive);
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
    if (listen(fd, 16) == SOCKET_ERROR) {
        die("listen");
    }
    return fd;
}

/* 把缓冲区里的完整行大写后发回。 */
static void echo_lines(struct conn *c) {
    int start = 0;
    for (int i = 0; i < c->used; i++) {
        if (c->buf[i] != '\n') {
            continue;
        }
        int len = i - start;
        for (int j = start; j < start + len; j++) {
            if (c->buf[j] >= 'a' && c->buf[j] <= 'z') {
                c->buf[j] = (char)(c->buf[j] - 'a' + 'A');
            }
        }
        send(c->fd, c->buf + start, len + 1, 0);
        printf("[conn %d] echoed %d bytes\n", c->id, len + 1);
        fflush(stdout);
        start = i + 1;
    }
    memmove(c->buf, c->buf + start, (size_t)(c->used - start));
    c->used -= start;
}

int main(int argc, char **argv) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return EXIT_FAILURE;
    }

    const char *port = argc > 1 ? argv[1] : "23467";
    SOCKET listener = tcp_listen(port);

    WSAEVENT listen_ev = WSACreateEvent();
    if (WSAEventSelect(listener, listen_ev, FD_ACCEPT | FD_CLOSE) ==
        SOCKET_ERROR) {
        die("WSAEventSelect");
    }

    HANDLE stop_ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    HANDLE thread = CreateThread(NULL, 0, stopper, stop_ev, 0, NULL);
    if (thread == NULL) {
        die("CreateThread");
    }

    printf("listening on 127.0.0.1:%s, will stop after 3 s\n", port);
    fflush(stdout);

    struct conn conns[MAX_CONNS];
    int nconns = 0;
    int next_id = 1;

    for (;;) {
        HANDLE handles[MAXIMUM_WAIT_OBJECTS];
        handles[0] = stop_ev;
        handles[1] = (HANDLE)listen_ev;
        for (int i = 0; i < nconns; i++) {
            handles[i + 2] = (HANDLE)conns[i].ev;
        }

        DWORD rc = WaitForMultipleObjects((DWORD)(nconns + 2), handles, FALSE,
                                          INFINITE);
        if (rc == WAIT_FAILED) {
            die("WaitForMultipleObjects");
        }
        if (rc == WAIT_OBJECT_0) {
            printf("stop event signaled, shutting down\n");
            fflush(stdout);
            break;
        }
        if (rc == WAIT_OBJECT_0 + 1) {
            WSANETWORKEVENTS ne;
            if (WSAEnumNetworkEvents(listener, listen_ev, &ne) ==
                SOCKET_ERROR) {
                die("WSAEnumNetworkEvents");
            }
            if (ne.lNetworkEvents & FD_ACCEPT) {
                for (;;) {
                    SOCKET c = accept(listener, NULL, NULL);
                    if (c == INVALID_SOCKET) {
                        break;
                    }
                    if (nconns == MAX_CONNS) {
                        closesocket(c);
                        continue;
                    }
                    WSAEVENT ev = WSACreateEvent();
                    if (WSAEventSelect(c, ev, FD_READ | FD_CLOSE) ==
                        SOCKET_ERROR) {
                        WSACloseEvent(ev);
                        closesocket(c);
                        continue;
                    }
                    conns[nconns].fd = c;
                    conns[nconns].ev = ev;
                    conns[nconns].used = 0;
                    conns[nconns].id = next_id++;
                    nconns++;
                    printf("[accept] conn %d connected\n", next_id - 1);
                    fflush(stdout);
                }
            }
            continue;
        }

        int idx = (int)(rc - WAIT_OBJECT_0) - 2;
        WSANETWORKEVENTS ne;
        if (WSAEnumNetworkEvents(conns[idx].fd, conns[idx].ev, &ne) ==
            SOCKET_ERROR) {
            die("WSAEnumNetworkEvents");
        }
        int closed = 0;
        if (ne.lNetworkEvents & FD_READ) {
            int room = LINE_CAP - conns[idx].used;
            int n = recv(conns[idx].fd, conns[idx].buf + conns[idx].used, room,
                         0);
            if (n == 0) {
                closed = 1;
            } else if (n == SOCKET_ERROR) {
                if (WSAGetLastError() != WSAEWOULDBLOCK) {
                    closed = 1;
                }
            } else {
                conns[idx].used += n;
                echo_lines(&conns[idx]);
            }
        }
        if (ne.lNetworkEvents & FD_CLOSE) {
            closed = 1;
        }

        if (closed) {
            printf("[close] conn %d closed\n", conns[idx].id);
            fflush(stdout);
            closesocket(conns[idx].fd);
            WSACloseEvent(conns[idx].ev);
            conns[idx] = conns[nconns - 1];
            nconns--;
        }
    }

    for (int i = 0; i < nconns; i++) {
        closesocket(conns[i].fd);
        WSACloseEvent(conns[i].ev);
    }
    closesocket(listener);
    WSACloseEvent(listen_ev);
    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    CloseHandle(stop_ev);
    WSACleanup();
    return 0;
}