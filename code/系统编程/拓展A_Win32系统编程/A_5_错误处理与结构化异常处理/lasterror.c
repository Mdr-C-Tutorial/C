#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <locale.h>
#include <stdio.h>

/* 把 Win32 错误码转成可读文字；消息来自系统，随系统界面语言变化 */
static void print_error(DWORD code)
{
    wchar_t *msg = NULL;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                 FORMAT_MESSAGE_FROM_SYSTEM |
                                 FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL, code, 0, (wchar_t *)&msg, 0, NULL);

    if (n == 0) {
        wprintf(L"error %lu: (no message)\n", (unsigned long)code);
        return;
    }
    /* 系统消息以 CRLF 结尾，去掉 */
    while (n > 0 && (msg[n - 1] == L'\r' || msg[n - 1] == L'\n')) {
        msg[--n] = L'\0';
    }
    wprintf(L"error %lu: %ls\n", (unsigned long)code, msg);
    LocalFree(msg);
}

/* 打开一个不存在的文件：错误 2 */
static DWORD open_missing(void)
{
    HANDLE h = CreateFileW(L"no_such_file.txt", GENERIC_READ, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h == INVALID_HANDLE_VALUE) {
        return GetLastError();
    }
    CloseHandle(h);
    return ERROR_SUCCESS;
}

/* 把目录当文件打开：这里故意不加 FILE_FLAG_BACKUP_SEMANTICS，得到错误 5 */
static DWORD open_directory(void)
{
    HANDLE h = CreateFileW(L"C:\\Windows", GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h == INVALID_HANDLE_VALUE) {
        return GetLastError();
    }
    CloseHandle(h);
    return ERROR_SUCCESS;
}

int main(void)
{
    DWORD code;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    code = open_missing();
    wprintf(L"CreateFileW: ");
    print_error(code);

    code = open_directory();
    wprintf(L"CreateFileW: ");
    print_error(code);
    return 0;
}