/* 命名管道回声服务器：消息模式，每个客户端一个线程，消息按 UTF-8 转码后显示。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

#define PIPE_NAME L"\\\\.\\pipe\\mdr_echo"
#define BUF_SIZE 4096

static LONG g_served = 0;
static LONG g_max_clients = 0;
static HANDLE g_all_done = NULL;

static void print_error(const wchar_t *what) {
    DWORD err = GetLastError();
    WCHAR *msg = NULL;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPWSTR)&msg, 0, NULL);
    fwprintf(stderr, L"%ls: error %lu: %ls", what, err, msg != NULL ? msg : L"");
    LocalFree(msg);
}

/* 把管道里的 UTF-8 字节转成宽字符再打印。 */
static void show_message(const char *bytes, DWORD len) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, bytes, (int)len, NULL, 0);
    wchar_t *wide = malloc(((size_t)wlen + 1) * sizeof *wide);

    if (wide == NULL) {
        return;
    }
    MultiByteToWideChar(CP_UTF8, 0, bytes, (int)len, wide, wlen);
    wide[wlen] = L'\0';
    wprintf(L"[server] got %lu bytes: \"%ls\"\n", len, wide);
    free(wide);
}

static DWORD WINAPI serve_client(LPVOID arg) {
    HANDLE pipe = arg;
    ULONG pid = 0;
    char buf[BUF_SIZE];

    if (GetNamedPipeClientProcessId(pipe, &pid)) {
        wprintf(L"[server] client connected, pid %lu\n", pid);
    }

    for (;;) {
        DWORD got = 0;
        if (!ReadFile(pipe, buf, sizeof buf, &got, NULL)) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
                break;
            }
            fwprintf(stderr, L"[server] ReadFile: error %lu\n", err);
            break;
        }
        show_message(buf, got);

        DWORD written = 0;
        if (!WriteFile(pipe, buf, got, &written, NULL)) {
            fwprintf(stderr, L"[server] WriteFile: error %lu\n", GetLastError());
            break;
        }
    }

    wprintf(L"[server] client disconnected\n");
    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);

    if (InterlockedIncrement(&g_served) == g_max_clients) {
        SetEvent(g_all_done);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (argc > 1) {
        g_max_clients = strtol(argv[1], NULL, 10);
    }

    g_all_done = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (g_all_done == NULL) {
        print_error(L"CreateEventW");
        return 1;
    }

    int instance = 0;
    while (g_max_clients == 0 || instance < g_max_clients) {
        instance++;
        DWORD open_mode = PIPE_ACCESS_DUPLEX;
        if (instance == 1) {
            open_mode |= FILE_FLAG_FIRST_PIPE_INSTANCE;
        }

        HANDLE pipe = CreateNamedPipeW(PIPE_NAME, open_mode,
                                       PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE |
                                           PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                       PIPE_UNLIMITED_INSTANCES, BUF_SIZE, BUF_SIZE,
                                       0, NULL);
        if (pipe == INVALID_HANDLE_VALUE) {
            print_error(L"CreateNamedPipeW");
            break;
        }

        wprintf(L"[server] instance %d waiting for a client\n", instance);
        if (!ConnectNamedPipe(pipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) {
            print_error(L"ConnectNamedPipe");
            CloseHandle(pipe);
            break;
        }

        HANDLE thread = CreateThread(NULL, 0, serve_client, pipe, 0, NULL);
        if (thread == NULL) {
            print_error(L"CreateThread");
            CloseHandle(pipe);
            break;
        }
        CloseHandle(thread);
    }

    if (g_max_clients > 0) {
        WaitForSingleObject(g_all_done, INFINITE);
    }
    wprintf(L"[server] served %ld client(s), exiting\n", g_served);
    CloseHandle(g_all_done);
    return 0;
}