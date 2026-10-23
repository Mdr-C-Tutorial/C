/* wsa_init.c —— WSAStartup 的必要性、版本协商与引用计数。
 *
 * gcc -std=c17 -Wall -Wextra wsa_init.c -o wsa_init.exe -lws2_32
 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
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

/* 只建一个 TCP 套接字再关掉，用来判断 Winsock 是否可用。 */
static void probe(const char *stage) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        printf("%s\n", stage);
        print_wsa_error("socket failed", WSAGetLastError());
        return;
    }
    printf("%s -> socket ok: %llu\n", stage, (unsigned long long)s);
    closesocket(s);
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    printf("sizeof(SOCKET) = %llu\n", (unsigned long long)sizeof(SOCKET));
    probe("before WSAStartup");

    WSADATA wsa;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
    printf("WSAStartup #1 = %d, version %u.%u, high %u.%u, \"%s\"\n", rc,
           LOBYTE(wsa.wVersion), HIBYTE(wsa.wVersion),
           LOBYTE(wsa.wHighVersion), HIBYTE(wsa.wHighVersion),
           wsa.szDescription);
    probe("after WSAStartup #1");

    printf("WSAStartup #2 = %d\n", WSAStartup(MAKEWORD(2, 2), &wsa));
    printf("WSACleanup #1 = %d\n", WSACleanup());
    probe("after one WSACleanup");
    printf("WSACleanup #2 = %d\n", WSACleanup());
    probe("after two WSACleanup");
    rc = WSACleanup();
    printf("WSACleanup #3 = %d\n", rc);
    print_wsa_error("WSACleanup failed", WSAGetLastError());
    return 0;
}