/* failcheck.c：句柄函数的失败值和 BOOL 函数的判断写法。 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    HANDLE in = CreateFileW(L"no_such_file.txt", GENERIC_READ, FILE_SHARE_READ,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    printf("CreateFileW   -> %p\n", (void *)in);
    printf("INVALID_HANDLE_VALUE = %p\n", (void *)INVALID_HANDLE_VALUE);

    HANDLE ev = OpenEventW(EVENT_ALL_ACCESS, FALSE, L"no_such_event");
    DWORD ev_err = GetLastError();
    printf("OpenEventW    -> %p, error %lu\n", (void *)ev, ev_err);

    WIN32_FIND_DATAW fd;
    HANDLE find = FindFirstFileW(L"no_such_dir\\*", &fd);
    printf("FindFirstFileW -> %p\n", (void *)find);

    /* BOOL 非零即真，成功时不一定等于 TRUE */
    DWORD attr = GetFileAttributesW(L".");
    printf("GetFileAttributesW(\".\") -> %lu\n", (unsigned long)attr);
    printf("GetFileAttributesW(\"no_such\") -> %lu\n",
           (unsigned long)GetFileAttributesW(L"no_such.txt"));

    if (in != INVALID_HANDLE_VALUE) {
        CloseHandle(in);
    }
    return 0;
}