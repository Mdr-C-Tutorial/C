/* 管道相关错误码一览：109、535、231、2。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>

#define NAME L"\\\\.\\pipe\\mdr_codes"

static void show(const char *what, DWORD err) {
    WCHAR *msg = NULL;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&msg, 0, NULL);
    printf("  %-22s = %lu: %ls", what, err, msg != NULL ? msg : L"\n");
    LocalFree(msg);
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    /* 1. 写端全部关闭之后的读端。 */
    HANDLE read_end = NULL;
    HANDLE write_end = NULL;
    char buf[16];
    DWORD got = 0;
    CreatePipe(&read_end, &write_end, NULL, 0);
    WriteFile(write_end, "hi", 2, &got, NULL);
    CloseHandle(write_end);
    ReadFile(read_end, buf, sizeof buf, &got, NULL);
    BOOL ok = ReadFile(read_end, buf, sizeof buf, &got, NULL);
    printf("read after all writers closed:\n");
    show("ERROR_BROKEN_PIPE", ok ? 0 : GetLastError());
    CloseHandle(read_end);

    /* 2. 客户端在 ConnectNamedPipe 之前就连上。 */
    HANDLE pipe = CreateNamedPipeW(NAME, PIPE_ACCESS_DUPLEX,
                                   PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1,
                                   4096, 4096, 0, NULL);
    HANDLE client = CreateFileW(NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0,
                                NULL);
    ok = ConnectNamedPipe(pipe, NULL);
    printf("ConnectNamedPipe with a client already connected:\n");
    show("ERROR_PIPE_CONNECTED", ok ? 0 : GetLastError());

    /* 3. 唯一的实例被占用时再开一个客户端。 */
    HANDLE second = CreateFileW(NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0,
                                NULL);
    printf("CreateFileW while the only instance is busy:\n");
    show("ERROR_PIPE_BUSY", GetLastError());

    /* 4. 管道名字根本不存在时 WaitNamedPipe 的错误。 */
    ok = WaitNamedPipeW(L"\\\\.\\pipe\\mdr_does_not_exist", 200);
    printf("WaitNamedPipeW for a pipe that does not exist:\n");
    show("ERROR_FILE_NOT_FOUND", ok ? 0 : GetLastError());

    if (second != INVALID_HANDLE_VALUE) {
        CloseHandle(second);
    }
    CloseHandle(client);
    CloseHandle(pipe);
    return 0;
}