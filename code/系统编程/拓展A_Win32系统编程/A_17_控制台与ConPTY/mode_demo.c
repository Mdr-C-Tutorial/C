/* mode_demo.c —— 保存、修改并恢复控制台输入模式。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -o mode_demo.exe mode_demo.c
 * 运行：mode_demo.exe，然后敲一行，回车。
 */
#include <windows.h>
#include <stdio.h>

static void show(const char *what, DWORD mode) {
    printf("%-20s 0x%04lX  line=%-3s echo=%-3s processed=%s\n", what, (unsigned long)mode,
           (mode & ENABLE_LINE_INPUT) ? "on" : "off", (mode & ENABLE_ECHO_INPUT) ? "on" : "off",
           (mode & ENABLE_PROCESSED_INPUT) ? "on" : "off");
}

int main(void) {
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD saved;
    if (!GetConsoleMode(in, &saved)) {
        printf("stdin is not a console (error %lu)\n", GetLastError());
        return 1;
    }
    show("original", saved);

    if (!SetConsoleMode(in, saved & ~(DWORD)(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT))) {
        printf("SetConsoleMode failed: %lu\n", GetLastError());
        return 1;
    }
    DWORD now;
    GetConsoleMode(in, &now);
    show("line/echo off", now);

    printf("type a line, it will not be echoed:\n");
    fflush(stdout);

    char buf[128];
    DWORD n = 0;
    if (!ReadFile(in, buf, sizeof buf, &n, NULL)) {
        printf("ReadFile failed: %lu\n", GetLastError());
        SetConsoleMode(in, saved);
        return 1;
    }
    printf("read %lu byte(s):", (unsigned long)n);
    for (DWORD i = 0; i < n; i++) {
        printf(" %02X", (unsigned char)buf[i]);
    }
    printf("\n");

    /* 退出前恢复，否则回到 shell 后输入不再回显。 */
    if (!SetConsoleMode(in, saved)) {
        printf("restore failed: %lu\n", GetLastError());
        return 1;
    }
    GetConsoleMode(in, &now);
    show("restored", now);
    return 0;
}