/* delete_on_close.c -- FILE_FLAG_DELETE_ON_CLOSE 让临时文件在最后一个句柄关闭时自动消失。 */
#include <windows.h>
#include <stdio.h>

int main(void) {
    HANDLE h;
    DWORD got = 0;

    h = CreateFileW(L"scratch.tmp", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
                    NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: %lu\n", GetLastError());
        return 1;
    }
    WriteFile(h, "temporary data", 14, &got, NULL);
    printf("while open:   file exists = %d\n",
           GetFileAttributesW(L"scratch.tmp") != INVALID_FILE_ATTRIBUTES);
    CloseHandle(h);
    printf("after close:  file exists = %d\n",
           GetFileAttributesW(L"scratch.tmp") != INVALID_FILE_ATTRIBUTES);
    return 0;
}