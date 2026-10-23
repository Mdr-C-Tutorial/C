/* 检查 FILE_FLAG_FIRST_PIPE_INSTANCE 与实例数量上限的错误码。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>

#define FIRST_NAME L"\\\\.\\pipe\\mdr_first"

static void show_error(const char *what) {
    DWORD err = GetLastError();
    WCHAR *msg = NULL;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPWSTR)&msg, 0, NULL);
    printf("%s: error %lu: %ls", what, err, msg != NULL ? msg : L"");
    LocalFree(msg);
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    HANDLE first = CreateNamedPipeW(FIRST_NAME, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                    PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, NULL);
    if (first == INVALID_HANDLE_VALUE) {
        show_error("first CreateNamedPipeW");
        return 1;
    }
    printf("first instance created\n");

    /* 同名再加 FIRST_PIPE_INSTANCE：文档说失败，错误 ERROR_ACCESS_DENIED。 */
    HANDLE second = CreateNamedPipeW(FIRST_NAME, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                     PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                     PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, NULL);
    if (second == INVALID_HANDLE_VALUE) {
        show_error("second CreateNamedPipeW with FIRST_PIPE_INSTANCE");
    } else {
        printf("second instance created (unexpected)\n");
        CloseHandle(second);
    }

    /* 不带 FIRST_PIPE_INSTANCE 时，同名实例可以继续创建。 */
    second = CreateNamedPipeW(FIRST_NAME, PIPE_ACCESS_DUPLEX,
                              PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                              PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, NULL);
    if (second == INVALID_HANDLE_VALUE) {
        show_error("second CreateNamedPipeW without the flag");
    } else {
        printf("second instance without the flag: ok\n");
        CloseHandle(second);
    }

    CloseHandle(first);
    return 0;
}