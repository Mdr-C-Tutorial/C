/* afunix_demo.c —— Windows 10 1803 起支持的 AF_UNIX 流套接字。
 *
 * gcc -std=c17 -Wall -Wextra afunix_demo.c -o afunix_demo.exe -lws2_32
 * 用法: afunix_demo [套接字文件路径]，默认 .\mdr_afunix.sock
 * 程序自己当服务器和客户端：绑定路径，起一个线程去连，回显一行后退出。
 */

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

static void die(const char *what) {
    fprintf(stderr, "%s: error %d\n", what, WSAGetLastError());
    exit(EXIT_FAILURE);
}

static const char *g_path;

static DWORD WINAPI client_thread(LPVOID arg) {
    (void)arg;
    Sleep(200);
    SOCKET c = socket(AF_UNIX, SOCK_STREAM, 0);
    if (c == INVALID_SOCKET) {
        printf("client: socket failed %d\n", WSAGetLastError());
        return 1;
    }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, g_path, sizeof addr.sun_path - 1);
    if (connect(c, (struct sockaddr *)&addr, sizeof addr) == SOCKET_ERROR) {
        printf("client: connect failed %d\n", WSAGetLastError());
        closesocket(c);
        return 1;
    }
    const char *msg = "hello over af_unix\n";
    send(c, msg, (int)strlen(msg), 0);
    char buf[128];
    int n = recv(c, buf, (int)sizeof buf, 0);
    if (n > 0) {
        printf("client received: %.*s", n, buf);
    }
    shutdown(c, SD_SEND);
    while (recv(c, buf, (int)sizeof buf, 0) > 0) {
    }
    closesocket(c);
    return 0;
}

int main(int argc, char **argv) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return EXIT_FAILURE;
    }

    g_path = argc > 1 ? argv[1] : "mdr_afunix.sock";
    DeleteFileA(g_path);

    SOCKET lfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (lfd == INVALID_SOCKET) {
        die("socket(AF_UNIX)");
    }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, g_path, sizeof addr.sun_path - 1);
    printf("bind path: %s (sun_path capacity %llu)\n", g_path,
           (unsigned long long)sizeof addr.sun_path);
    if (bind(lfd, (struct sockaddr *)&addr, sizeof addr) == SOCKET_ERROR) {
        die("bind");
    }
    if (listen(lfd, 4) == SOCKET_ERROR) {
        die("listen");
    }
    printf("listening on AF_UNIX socket\n");
    fflush(stdout);

    HANDLE thread = CreateThread(NULL, 0, client_thread, NULL, 0, NULL);
    if (thread == NULL) {
        die("CreateThread");
    }

    SOCKET c = accept(lfd, NULL, NULL);
    if (c == INVALID_SOCKET) {
        die("accept");
    }
    char buf[128];
    int n = recv(c, buf, (int)sizeof buf, 0);
    if (n > 0) {
        printf("server received: %.*s", n, buf);
        for (int i = 0; i < n; i++) {
            if (buf[i] >= 'a' && buf[i] <= 'z') {
                buf[i] = (char)(buf[i] - 'a' + 'A');
            }
        }
        send(c, buf, n, 0);
    }
    shutdown(c, SD_SEND);
    while (recv(c, buf, (int)sizeof buf, 0) > 0) {
    }
    closesocket(c);
    closesocket(lfd);
    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);

    DeleteFileA(g_path);
    WSACleanup();
    return 0;
}