#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

static void show(DWORD code)
{
    wchar_t *msg = NULL;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                 FORMAT_MESSAGE_FROM_SYSTEM |
                                 FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                             (wchar_t *)&msg, 0, NULL);

    if (n == 0) {
        printf("0x%08lX  (no message)\n", (unsigned long)code);
        return;
    }
    while (n > 0 && (msg[n - 1] == L'\r' || msg[n - 1] == L'\n')) {
        msg[--n] = L'\0';
    }
    printf("0x%08lX  %ls\n", (unsigned long)code, msg);
    LocalFree(msg);
}

int main(void)
{
    HRESULT hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    HANDLE a;
    HANDLE b;
    FILE *fp;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    printf("HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) = 0x%08lX, FAILED=%d, "
           "SUCCEEDED(S_OK)=%d\n",
           (unsigned long)hr, FAILED(hr), SUCCEEDED(S_OK));

    show(ERROR_FILE_NOT_FOUND);
    show((DWORD)hr);
    show(ERROR_ACCESS_DENIED);
    show(ERROR_ALREADY_EXISTS);
    show(0xC0000005u);
    show(10061u);
    show(0x80004005u);

    fp = _wfopen(L"no_such_file.txt", L"rb");
    if (fp == NULL) {
        printf("_wfopen failed: errno=%d (%s)\n", errno, strerror(errno));
    } else {
        fclose(fp);
    }

    /* 命名对象已存在时，创建函数成功，但把最后错误码设成 183 */
    SetLastError(0);
    a = CreateEventW(NULL, FALSE, FALSE, L"Local\\MdrTutorial.LastErrorTest");
    printf("CreateEventW #1: last_error=%lu\n", (unsigned long)GetLastError());
    b = CreateEventW(NULL, FALSE, FALSE, L"Local\\MdrTutorial.LastErrorTest");
    printf("CreateEventW #2: last_error=%lu\n", (unsigned long)GetLastError());
    CloseHandle(a);
    CloseHandle(b);
    return 0;
}