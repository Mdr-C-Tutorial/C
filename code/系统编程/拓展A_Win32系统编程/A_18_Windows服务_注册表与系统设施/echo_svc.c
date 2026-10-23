/* MdrEcho：把 A.15 的回声服务器包装成 Windows 服务。
 *
 * 服务模式由 SCM 启动；从命令行直接运行时自动切换到控制台模式，便于调试。
 * 监听地址固定为 127.0.0.1，端口读自 HKCU\Software\MdrTutorial\Echo 的 Port 值。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -o echo_svc.exe echo_svc.c -lws2_32
 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <locale.h>
#include <stdio.h>
#include <wchar.h>

#define SERVICE_NAME L"MdrEcho"
#define CONFIG_KEY L"Software\\MdrTutorial\\Echo"
#define CONFIG_VALUE L"Port"
#define DEFAULT_PORT 23456
#define BUF_SIZE 4096

static HANDLE g_stop = NULL;
static SERVICE_STATUS_HANDLE g_status_handle = NULL;
static SERVICE_STATUS g_status;
static SOCKET g_listen = INVALID_SOCKET;
static volatile LONG g_clients = 0;

/* 控制台模式下 g_status_handle 为 NULL，所有状态报告变成空操作。 */
static void report_status(DWORD state, DWORD wait_hint) {
    static DWORD checkpoint = 1;

    if (g_status_handle == NULL) {
        return;
    }
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwCurrentState = state;
    g_status.dwControlsAccepted =
        (state == SERVICE_RUNNING) ? (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN) : 0;
    g_status.dwWin32ExitCode = NO_ERROR;
    g_status.dwServiceSpecificExitCode = 0;
    g_status.dwWaitHint = wait_hint;
    if (state == SERVICE_START_PENDING || state == SERVICE_STOP_PENDING) {
        g_status.dwCheckPoint = checkpoint++;
    } else {
        g_status.dwCheckPoint = 0;
        checkpoint = 1;
    }
    SetServiceStatus(g_status_handle, &g_status);
}

/* 服务模式的 HandlerEx；控制台模式由 console_ctrl 承担同样的职责。 */
static DWORD WINAPI service_ctrl(DWORD control, DWORD event_type, void *event_data,
                                 void *context) {
    (void)event_type;
    (void)event_data;
    (void)context;

    if (control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN) {
        report_status(SERVICE_STOP_PENDING, 5000);
        SetEvent(g_stop);
    }
    return NO_ERROR;
}

static BOOL WINAPI console_ctrl(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT ||
        type == CTRL_SHUTDOWN_EVENT) {
        wprintf(L"[svc] console control %lu, stopping\n", (unsigned long)type);
        SetEvent(g_stop);
        return TRUE;
    }
    return FALSE;
}

static unsigned short read_port(void) {
    DWORD port = 0;
    DWORD size = sizeof port;
    LSTATUS st = RegGetValueW(HKEY_CURRENT_USER, CONFIG_KEY, CONFIG_VALUE, RRF_RT_REG_DWORD,
                              NULL, &port, &size);

    if (st == ERROR_SUCCESS && port > 0 && port < 65536) {
        wprintf(L"[svc] port %lu from HKCU\\%ls\\%ls\n", (unsigned long)port, CONFIG_KEY,
                CONFIG_VALUE);
        return (unsigned short)port;
    }
    wprintf(L"[svc] no usable Port value (status %ld), falling back to %d\n", (long)st,
            DEFAULT_PORT);
    return DEFAULT_PORT;
}

/* 每个客户端在同一个线程里顺序服务：读多少回多少，直到对端关闭。 */
static void serve_client(SOCKET client) {
    struct sockaddr_in peer;
    int peer_len = sizeof peer;
    char addr[INET_ADDRSTRLEN] = "?";
    char buf[BUF_SIZE];
    unsigned long long total = 0;

    if (getpeername(client, (struct sockaddr *)&peer, &peer_len) == 0) {
        inet_ntop(AF_INET, &peer.sin_addr, addr, sizeof addr);
    }
    wprintf(L"[svc] client %hs:%u connected\n", addr, (unsigned)ntohs(peer.sin_port));

    for (;;) {
        fd_set set;
        struct timeval tv;

        if (WaitForSingleObject(g_stop, 0) == WAIT_OBJECT_0) {
            break;
        }
        FD_ZERO(&set);
        FD_SET(client, &set);
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        if (select(0, &set, NULL, NULL, &tv) <= 0) {
            continue;
        }
        int got = recv(client, buf, sizeof buf, 0);
        if (got <= 0) {
            break;
        }
        int sent = 0;
        while (sent < got) {
            int n = send(client, buf + sent, got - sent, 0);
            if (n <= 0) {
                break;
            }
            sent += n;
        }
        total += (unsigned long long)got;
    }
    wprintf(L"[svc] client %hs disconnected, echoed %llu bytes\n", addr, total);
    InterlockedIncrement(&g_clients);
}

static DWORD WINAPI echo_loop(LPVOID arg) {
    (void)arg;

    while (WaitForSingleObject(g_stop, 0) == WAIT_TIMEOUT) {
        fd_set set;
        struct timeval tv;
        int ready;

        FD_ZERO(&set);
        FD_SET(g_listen, &set);
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        ready = select(0, &set, NULL, NULL, &tv);
        if (ready > 0) {
            SOCKET client = accept(g_listen, NULL, NULL);
            if (client == INVALID_SOCKET) {
                break;
            }
            serve_client(client);
            closesocket(client);
        }
    }
    return 0;
}

/* 初始化 Winsock、读端口、绑定并监听。失败返回 0。 */
static int echo_init(void) {
    WSADATA wsa;
    SOCKADDR_IN addr;
    unsigned short port;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        wprintf(L"[svc] WSAStartup failed\n");
        return 0;
    }
    port = read_port();

    g_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen == INVALID_SOCKET) {
        wprintf(L"[svc] socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 0;
    }

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (bind(g_listen, (SOCKADDR *)&addr, sizeof addr) == SOCKET_ERROR) {
        wprintf(L"[svc] bind on 127.0.0.1:%u failed: %d\n", (unsigned)port, WSAGetLastError());
        closesocket(g_listen);
        g_listen = INVALID_SOCKET;
        WSACleanup();
        return 0;
    }
    if (listen(g_listen, SOMAXCONN) == SOCKET_ERROR) {
        wprintf(L"[svc] listen on 127.0.0.1:%u failed: %d\n", (unsigned)port, WSAGetLastError());
        closesocket(g_listen);
        g_listen = INVALID_SOCKET;
        WSACleanup();
        return 0;
    }
    wprintf(L"[svc] listening on 127.0.0.1:%u\n", (unsigned)port);
    return 1;
}

static void echo_shutdown(void) {
    if (g_listen != INVALID_SOCKET) {
        closesocket(g_listen);
        g_listen = INVALID_SOCKET;
    }
}

/* 服务模式与控制台模式共用的主体。 */
static int run_echo(void) {
    HANDLE worker;

    if (!echo_init()) {
        report_status(SERVICE_STOPPED, 0);
        return 1;
    }

    worker = CreateThread(NULL, 0, echo_loop, NULL, 0, NULL);
    if (worker == NULL) {
        wprintf(L"[svc] CreateThread failed: %lu\n", (unsigned long)GetLastError());
        echo_shutdown();
        report_status(SERVICE_STOPPED, 0);
        return 1;
    }

    report_status(SERVICE_RUNNING, 0);
    wprintf(L"[svc] running\n");

    WaitForSingleObject(g_stop, INFINITE);
    /* 工作线程每 200 毫秒检查一次停止事件，这里等它退出再释放套接字。 */
    WaitForSingleObject(worker, INFINITE);
    CloseHandle(worker);
    echo_shutdown();
    WSACleanup();

    wprintf(L"[svc] stopped, %ld client(s) served\n", (long)g_clients);
    report_status(SERVICE_STOPPED, 0);
    return 0;
}

static void WINAPI service_main(DWORD argc, LPWSTR *argv) {
    (void)argc;
    (void)argv;

    g_status_handle = RegisterServiceCtrlHandlerExW(SERVICE_NAME, service_ctrl, NULL);
    if (g_status_handle == NULL) {
        return;
    }
    report_status(SERVICE_START_PENDING, 3000);

    g_stop = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (g_stop == NULL) {
        report_status(SERVICE_STOPPED, 0);
        return;
    }
    run_echo();
    CloseHandle(g_stop);
}

int main(void) {
    SERVICE_TABLE_ENTRYW table[] = {
        {(LPWSTR)SERVICE_NAME, service_main},
        {NULL, NULL},
    };

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);

    if (StartServiceCtrlDispatcherW(table)) {
        return 0;
    }

    DWORD err = GetLastError();
    if (err != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
        wprintf(L"[svc] StartServiceCtrlDispatcherW failed: %lu\n", (unsigned long)err);
        return 1;
    }

    wprintf(L"[svc] not started by the SCM (error %lu), console mode\n", (unsigned long)err);
    g_stop = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (g_stop == NULL) {
        wprintf(L"[svc] CreateEventW failed: %lu\n", (unsigned long)GetLastError());
        return 1;
    }
    SetConsoleCtrlHandler(console_ctrl, TRUE);
    int rc = run_echo();
    CloseHandle(g_stop);
    return rc;
}