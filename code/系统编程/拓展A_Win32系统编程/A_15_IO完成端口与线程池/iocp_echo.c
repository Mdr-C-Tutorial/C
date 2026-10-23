/* IOCP 回声服务器：主线程 accept 并把新连接关联到完成端口，工作线程处理完成包。
 * 用法: iocp_echo [运行秒数]，默认 10 秒。只监听 127.0.0.1:23456。
 *
 * gcc -std=c17 -Wall -Wextra iocp_echo.c -o iocp_echo.exe -lws2_32
 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

#define PORT 23456
#define BUF_SIZE 4096
#define MAX_CONN 256

enum { OP_RECV, OP_SEND };

struct conn {
    OVERLAPPED ov;
    SOCKET sock;
    WSABUF buf;
    char data[BUF_SIZE];
    int op;
    int id;
    LONG closing;
    unsigned long long total;
};

static HANDLE g_port;
static SRWLOCK g_lock = SRWLOCK_INIT;
static struct conn *g_conns[MAX_CONN];
static int g_conn_count;
static LONG g_live;
static HANDLE g_all_closed;

/* 从活动连接表里摘掉 c。 */
static void conn_remove(struct conn *c) {
    AcquireSRWLockExclusive(&g_lock);
    for (int i = 0; i < g_conn_count; i++) {
        if (g_conns[i] == c) {
            g_conns[i] = g_conns[g_conn_count - 1];
            g_conn_count--;
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
}

/* 只由工作线程调用：摘表、关闭套接字、释放上下文。 */
static void conn_release(struct conn *c) {
    conn_remove(c);
    closesocket(c->sock);
    printf("conn %d: 关闭，累计回送 %llu 字节\n", c->id, c->total);
    free(c);
    if (InterlockedDecrement(&g_live) == 0) {
        SetEvent(g_all_closed);
    }
}

/* 在锁内检查 closing 再发起下一次 I/O，避免与关闭过程抢跑。
 * 返回 0 表示请求已提交（完成包一定会到达）；-1 表示该连接正在关闭。 */
static int post_io(struct conn *c, int op, DWORD len) {
    DWORD n = 0;
    DWORD flags = 0;
    int rc;

    AcquireSRWLockExclusive(&g_lock);
    if (c->closing) {
        ReleaseSRWLockExclusive(&g_lock);
        return -1;
    }
    c->op = op;
    c->buf.buf = c->data;
    c->buf.len = op == OP_RECV ? BUF_SIZE : len;
    ZeroMemory(&c->ov, sizeof c->ov);
    if (op == OP_RECV) {
        rc = WSARecv(c->sock, &c->buf, 1, &n, &flags, &c->ov, NULL);
    } else {
        rc = WSASend(c->sock, &c->buf, 1, &n, 0, &c->ov, NULL);
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (rc == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        return -1;
    }
    return 0;
}

static DWORD WINAPI worker(LPVOID arg) {
    (void)arg;

    for (;;) {
        DWORD n;
        ULONG_PTR key;
        OVERLAPPED *ov;
        struct conn *c;

        if (!GetQueuedCompletionStatus(g_port, &n, &key, &ov, INFINITE)) {
            DWORD err = GetLastError();

            if (ov == NULL) {
                return 0; /* 端口句柄已关闭或没有完成包可取 */
            }
            c = CONTAINING_RECORD(ov, struct conn, ov);
            if (err == ERROR_OPERATION_ABORTED) {
                printf("worker %lu: conn %d 的挂起 I/O 被取消\n",
                       GetCurrentThreadId(), c->id);
            } else {
                printf("conn %d: I/O 失败，错误 %lu\n", c->id, err);
            }
            conn_release(c);
            continue;
        }

        /* 退出包：完成键和 OVERLAPPED 指针都是 0 */
        if (key == 0 && ov == NULL) {
            return 0;
        }

        c = CONTAINING_RECORD(ov, struct conn, ov);

        if (c->op == OP_RECV) {
            if (n == 0) {
                printf("worker %lu: conn %d 对端关闭\n", GetCurrentThreadId(),
                       c->id);
                conn_release(c);
                continue;
            }
            printf("worker %lu: conn %d 收到 %lu 字节\n", GetCurrentThreadId(),
                   c->id, (unsigned long)n);
            if (post_io(c, OP_SEND, n) < 0) {
                conn_release(c);
            }
        } else {
            c->total += n;
            printf("worker %lu: conn %d 回送 %lu 字节\n", GetCurrentThreadId(),
                   c->id, (unsigned long)n);
            if (post_io(c, OP_RECV, 0) < 0) {
                conn_release(c);
            }
        }
    }
}

int main(int argc, char **argv) {
    WSADATA wsa;
    SOCKET listen_sock;
    struct sockaddr_in addr = {0};
    int seconds = argc > 1 ? atoi(argv[1]) : 10;
    int worker_count;
    SYSTEM_INFO si;
    HANDLE *threads;
    ULONGLONG deadline;
    int next_id = 1;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup 失败\n");
        return 1;
    }

    g_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (g_port == NULL) {
        printf("CreateIoCompletionPort 失败: %lu\n", GetLastError());
        return 1;
    }
    g_all_closed = CreateEventW(NULL, TRUE, FALSE, NULL);

    GetSystemInfo(&si);
    worker_count = (int)si.dwNumberOfProcessors;
    threads = calloc((size_t)worker_count, sizeof *threads);
    for (int i = 0; i < worker_count; i++) {
        threads[i] = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    }
    printf("工作线程 %d 个，监听 127.0.0.1:%d，运行 %d 秒\n", worker_count,
           PORT, seconds);

    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof addr) ==
        SOCKET_ERROR) {
        printf("bind 失败: %d\n", WSAGetLastError());
        return 1;
    }
    listen(listen_sock, SOMAXCONN);

    deadline = GetTickCount64() + (ULONGLONG)seconds * 1000;
    while (GetTickCount64() < deadline) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(listen_sock, &set);
        struct timeval tv = {0, 200000};
        if (select(0, &set, NULL, NULL, &tv) <= 0) {
            continue;
        }

        SOCKET s = accept(listen_sock, NULL, NULL);
        if (s == INVALID_SOCKET) {
            continue;
        }
        struct conn *c = calloc(1, sizeof *c);
        if (c == NULL) {
            closesocket(s);
            continue;
        }
        c->sock = s;
        c->id = next_id++;

        /* 关联句柄，完成键就是这个连接的上下文指针 */
        if (CreateIoCompletionPort((HANDLE)s, g_port, (ULONG_PTR)c, 0) == NULL) {
            printf("关联失败: %lu\n", GetLastError());
            closesocket(s);
            free(c);
            continue;
        }
        AcquireSRWLockExclusive(&g_lock);
        g_conns[g_conn_count++] = c;
        ReleaseSRWLockExclusive(&g_lock);
        InterlockedIncrement(&g_live);
        printf("main: 接受 conn %d\n", c->id);

        if (post_io(c, OP_RECV, 0) < 0) {
            conn_release(c);
        }
    }

    closesocket(listen_sock);
    printf("main: 时间到，取消所有连接的挂起 I/O\n");

    /* 挂起的 I/O 以 ERROR_OPERATION_ABORTED 完成，由工作线程负责释放 */
    AcquireSRWLockExclusive(&g_lock);
    for (int i = 0; i < g_conn_count; i++) {
        InterlockedExchange(&g_conns[i]->closing, 1);
        CancelIoEx((HANDLE)g_conns[i]->sock, NULL);
    }
    ReleaseSRWLockExclusive(&g_lock);

    if (WaitForSingleObject(g_all_closed, 5000) == WAIT_OBJECT_0) {
        printf("main: 全部连接已释放\n");
    } else {
        printf("main: 等待连接释放超时\n");
    }

    for (int i = 0; i < worker_count; i++) {
        PostQueuedCompletionStatus(g_port, 0, 0, NULL);
    }
    WaitForMultipleObjects((DWORD)worker_count, threads, TRUE, INFINITE);
    printf("main: %d 个工作线程全部退出\n", worker_count);
    for (int i = 0; i < worker_count; i++) {
        CloseHandle(threads[i]);
    }
    free(threads);
    CloseHandle(g_all_closed);
    CloseHandle(g_port);
    WSACleanup();
    return 0;
}