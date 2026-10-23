/* sock_errors.c —— 套接字调用的错误码与系统给出的文字。
 *
 * gcc -std=c17 -Wall -Wextra sock_errors.c -o sock_errors.exe -lws2_32
 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>

static void print_wsa_error(const char *what, int code) {
    wchar_t *msg = NULL;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                 FORMAT_MESSAGE_FROM_SYSTEM |
                                 FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL, (DWORD)code,
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                             (LPWSTR)&msg, 0, NULL);
    if (n == 0) {
        printf("  %s: %d\n", what, code);
        return;
    }
    while (n > 0 && (msg[n - 1] == L'\r' || msg[n - 1] == L'\n')) {
        msg[--n] = L'\0';
    }
    printf("  %s: %d (%ls)\n", what, code, msg);
    LocalFree(msg);
}

/* 连一次 127.0.0.1:port，失败时返回错误码。 */
static int connect_err(unsigned short port) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        return WSAGetLastError();
    }
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(port)};
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int rc = connect(s, (struct sockaddr *)&addr, sizeof addr);
    int err = rc == SOCKET_ERROR ? WSAGetLastError() : 0;
    closesocket(s);
    return err;
}

/* 绑一个套接字到 127.0.0.1:port，返回套接字；失败返回 INVALID_SOCKET。 */
static SOCKET bind_one(unsigned short port, int *err) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(port)};
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(s, (struct sockaddr *)&addr, sizeof addr) == SOCKET_ERROR) {
        *err = WSAGetLastError();
        closesocket(s);
        return INVALID_SOCKET;
    }
    *err = 0;
    return s;
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return 1;
    }

    printf("1) 连接一个没人监听的端口\n");
    int err = connect_err(23457);
    print_wsa_error("WSAGetLastError()", err);
    printf("  errno = %d\n", errno);

    printf("2) 两个套接字绑定同一端口（第二个不设任何选项）\n");
    SOCKET a = bind_one(23458, &err);
    bind_one(23458, &err);
    print_wsa_error("bind b", err);

    printf("3) 同一个套接字重复绑定\n");
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(23458)};
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(a, (struct sockaddr *)&addr, sizeof addr) == SOCKET_ERROR) {
        print_wsa_error("bind a again", WSAGetLastError());
    }

    printf("4) 对已关闭的套接字调用 send\n");
    SOCKET c = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    closesocket(c);
    char byte = 'x';
    if (send(c, &byte, 1, 0) == SOCKET_ERROR) {
        print_wsa_error("send", WSAGetLastError());
    }

    printf("5) 同一次失败里分别用两个函数取错误码\n");
    SOCKET probe = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connect(probe, (struct sockaddr *)&addr, sizeof addr) == SOCKET_ERROR) {
        int wsa = WSAGetLastError();
        int win32 = (int)GetLastError();
        printf("  WSAGetLastError = %d, GetLastError = %d, 相同 = %d\n", wsa,
               win32, wsa == win32);
    }
    closesocket(probe);

    closesocket(a);
    WSACleanup();
    return 0;
}