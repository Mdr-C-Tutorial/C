/* 单线程重叠管道服务器：同时服务两个 \\.\pipe\mdr_echo 实例，
   每个实例一个状态机（连接中 -> 读 -> 写 -> 断开重连）。 */
#include <windows.h>
#include <stdio.h>

#define INSTANCES 2
#define BUF_SIZE 4096

enum { ST_CONNECTING, ST_READING, ST_WRITING, ST_DEAD };

typedef struct pipe_inst {
    HANDLE h;
    HANDLE ev;
    OVERLAPPED ov;
    char buf[BUF_SIZE];
    DWORD len;
    int state;
    int pending;
    int index;
} pipe_inst;

static pipe_inst g_inst[INSTANCES];

static void drop_client(pipe_inst *p, const char *why, DWORD err) {
    if (err != 0) {
        printf("instance %d: %s (error %lu)\n", p->index, why, err);
    } else {
        printf("instance %d: %s\n", p->index, why);
    }
    DisconnectNamedPipe(p->h);
    p->state = ST_CONNECTING;
    p->pending = 0;
}

/* 从当前状态开始一直发起操作，直到某一步真的挂起为止。 */
static void run(pipe_inst *p) {
    for (;;) {
        DWORD n = 0;
        DWORD err;
        BOOL ok;

        if (p->state == ST_DEAD) {
            return;
        }
        ResetEvent(p->ev);
        switch (p->state) {
        case ST_CONNECTING:
            ok = ConnectNamedPipe(p->h, &p->ov);
            if (ok) {
                printf("instance %d: client connected\n", p->index);
                p->state = ST_READING;
                continue;
            }
            err = GetLastError();
            if (err == ERROR_PIPE_CONNECTED) {
                printf("instance %d: client connected\n", p->index);
                p->state = ST_READING;
                continue;
            }
            if (err == ERROR_IO_PENDING) {
                p->pending = 1;
                return;
            }
            printf("instance %d: ConnectNamedPipe failed, error %lu\n",
                   p->index, err);
            p->state = ST_DEAD;
            return;
        case ST_READING:
            ok = ReadFile(p->h, p->buf, BUF_SIZE, &n, &p->ov);
            if (!ok && GetLastError() == ERROR_IO_PENDING) {
                p->pending = 1;
                return;
            }
            if (!ok) {
                drop_client(p, "client went away", GetLastError());
                continue;
            }
            if (n == 0) {
                drop_client(p, "client closed the pipe", 0);
                continue;
            }
            p->len = n;
            printf("instance %d: read %lu bytes\n", p->index, n);
            p->state = ST_WRITING;
            continue;
        case ST_WRITING:
            ok = WriteFile(p->h, p->buf, p->len, &n, &p->ov);
            if (!ok && GetLastError() == ERROR_IO_PENDING) {
                p->pending = 1;
                return;
            }
            if (!ok) {
                drop_client(p, "write failed", GetLastError());
                continue;
            }
            printf("instance %d: wrote %lu bytes back\n", p->index, n);
            p->state = ST_READING;
            continue;
        default:
            return;
        }
    }
}

/* 事件有信号之后调用：取结果、推进状态、发起下一个操作。 */
static void resume(pipe_inst *p) {
    DWORD n = 0;
    DWORD err = 0;
    BOOL ok;

    if (p->pending) {
        ok = GetOverlappedResult(p->h, &p->ov, &n, FALSE);
        err = ok ? 0 : GetLastError();
        p->pending = 0;
        switch (p->state) {
        case ST_CONNECTING:
            if (!ok) {
                drop_client(p, "connect failed", err);
                break;
            }
            printf("instance %d: client connected\n", p->index);
            p->state = ST_READING;
            break;
        case ST_READING:
            if (!ok) {
                drop_client(p, "client went away", err);
                break;
            }
            if (n == 0) {
                drop_client(p, "client closed the pipe", 0);
                break;
            }
            p->len = n;
            printf("instance %d: read %lu bytes\n", p->index, n);
            p->state = ST_WRITING;
            break;
        case ST_WRITING:
            if (!ok) {
                drop_client(p, "write failed", err);
                break;
            }
            printf("instance %d: wrote %lu bytes back\n", p->index, n);
            p->state = ST_READING;
            break;
        default:
            break;
        }
    }
    run(p);
}

int wmain(int argc, wchar_t **argv) {
    HANDLE timer;
    HANDLE handles[INSTANCES + 1];
    LARGE_INTEGER due;
    int seconds = 10;
    int i;

    if (argc > 1) {
        seconds = _wtoi(argv[1]);
        if (seconds <= 0) {
            seconds = 10;
        }
    }

    timer = CreateWaitableTimerW(NULL, TRUE, NULL);
    if (timer == NULL) {
        printf("CreateWaitableTimerW failed: %lu\n", GetLastError());
        return 1;
    }
    due.QuadPart = -((LONGLONG)seconds * 10000000);
    if (!SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE)) {
        printf("SetWaitableTimer failed: %lu\n", GetLastError());
        CloseHandle(timer);
        return 1;
    }
    handles[0] = timer;
    printf("serving %d instances of the mdr_echo pipe for %d seconds\n",
           INSTANCES, seconds);

    for (i = 0; i < INSTANCES; i++) {
        pipe_inst *p = &g_inst[i];

        p->index = i;
        p->len = 0;
        p->pending = 0;
        p->ev = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (p->ev == NULL) {
            printf("CreateEventW failed: %lu\n", GetLastError());
            return 1;
        }
        p->ov.hEvent = p->ev;
        p->h = CreateNamedPipeW(L"\\\\.\\pipe\\mdr_echo",
                                PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE |
                                    PIPE_WAIT,
                                INSTANCES, BUF_SIZE, BUF_SIZE, 0, NULL);
        if (p->h == INVALID_HANDLE_VALUE) {
            printf("CreateNamedPipeW failed: %lu\n", GetLastError());
            return 1;
        }
        handles[i + 1] = p->ev;
        p->state = ST_CONNECTING;
        run(p);
    }

    for (;;) {
        DWORD rc = WaitForMultipleObjects(INSTANCES + 1, handles, FALSE,
                                          INFINITE);

        if (rc == WAIT_OBJECT_0) {
            printf("timer expired, shutting down\n");
            break;
        }
        if (rc > WAIT_OBJECT_0 && rc < WAIT_OBJECT_0 + INSTANCES + 1) {
            resume(&g_inst[rc - WAIT_OBJECT_0 - 1]);
            continue;
        }
        printf("WaitForMultipleObjects failed: %lu\n", GetLastError());
        break;
    }

    for (i = 0; i < INSTANCES; i++) {
        pipe_inst *p = &g_inst[i];
        DWORD n = 0;

        if (p->pending) {
            CancelIoEx(p->h, &p->ov);
            if (GetOverlappedResult(p->h, &p->ov, &n, TRUE)) {
                printf("instance %d: cancelled operation had completed\n", i);
            } else {
                printf("instance %d: cancelled operation ended with error %lu\n",
                       i, GetLastError());
            }
        }
        DisconnectNamedPipe(p->h);
        CloseHandle(p->h);
        CloseHandle(p->ev);
    }
    CloseHandle(timer);
    printf("server stopped\n");
    return 0;
}