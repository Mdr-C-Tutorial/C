/* isconsole.c —— 判断标准句柄是否连接到控制台。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -o isconsole.exe isconsole.c
 * 运行：isconsole.exe            （在控制台里）
 *       isconsole.exe > out.txt  （标准输出被重定向）
 *       isconsole.exe | more     （标准输出是管道）
 */
#include <windows.h>
#include <stdio.h>

static const char *type_name(DWORD type) {
    switch (type) {
    case FILE_TYPE_CHAR:
        return "FILE_TYPE_CHAR";
    case FILE_TYPE_DISK:
        return "FILE_TYPE_DISK";
    case FILE_TYPE_PIPE:
        return "FILE_TYPE_PIPE";
    case FILE_TYPE_UNKNOWN:
        return "FILE_TYPE_UNKNOWN";
    default:
        return "?";
    }
}

static void report(const char *name, DWORD std_id) {
    HANDLE h = GetStdHandle(std_id);
    DWORD type = GetFileType(h);
    DWORD mode = 0;
    BOOL is_console = GetConsoleMode(h, &mode);

    printf("%-6s %-18s console: %s", name, type_name(type), is_console ? "yes" : "no");
    if (is_console) {
        printf(" (mode 0x%04lX)", (unsigned long)mode);
    }
    printf("\n");
}

int main(void) {
    report("stdin", STD_INPUT_HANDLE);
    report("stdout", STD_OUTPUT_HANDLE);
    report("stderr", STD_ERROR_HANDLE);

    /* 标准句柄被重定向时，CONOUT$ 仍然指向控制台的活动屏幕缓冲区。 */
    HANDLE conout = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                OPEN_EXISTING, 0, NULL);
    if (conout != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(conout, &mode)) {
            printf("CONOUT$ is a console (mode 0x%04lX)\n", (unsigned long)mode);
        }
        CloseHandle(conout);
    } else {
        printf("no console to open as CONOUT$ (error %lu)\n", GetLastError());
    }
    return 0;
}