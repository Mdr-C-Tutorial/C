/* acceptex_ptr.c —— 用 WSAIoctl 取得 Winsock 扩展函数 AcceptEx 的指针。
 *
 * gcc -std=c17 -Wall -Wextra acceptex_ptr.c -o acceptex_ptr.exe -lws2_32 -lmswsock
 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#include <locale.h>
#include <stdio.h>

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return 1;
    }

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    LPFN_ACCEPTEX accept_ex = NULL;
    GUID guid = WSAID_ACCEPTEX;
    DWORD bytes = 0;
    int rc = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid,
                      sizeof guid, &accept_ex, sizeof accept_ex, &bytes, NULL,
                      NULL);
    printf("WSAID_ACCEPTEX: rc = %d, bytes = %lu, pointer = %p\n", rc,
           (unsigned long)bytes, (void *)accept_ex);

    LPFN_GETACCEPTEXSOCKADDRS get_addrs = NULL;
    GUID guid2 = WSAID_GETACCEPTEXSOCKADDRS;
    rc = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid2, sizeof guid2,
                  &get_addrs, sizeof get_addrs, &bytes, NULL, NULL);
    printf("WSAID_GETACCEPTEXSOCKADDRS: rc = %d, pointer = %p\n", rc,
           (void *)get_addrs);

    closesocket(s);
    WSACleanup();
    return 0;
}