/* 演示 ERROR_BROKEN_PIPE：写端全部关闭后再读管道。 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    HANDLE read_end = NULL;
    HANDLE write_end = NULL;
    char buf[16];
    DWORD got = 0;

    if (!CreatePipe(&read_end, &write_end, NULL, 0)) {
        printf("CreatePipe: error %lu\n", GetLastError());
        return 1;
    }

    WriteFile(write_end, "hi", 2, &got, NULL);
    printf("wrote %lu bytes, then closing the write end\n", got);
    CloseHandle(write_end);

    BOOL ok = ReadFile(read_end, buf, sizeof buf, &got, NULL);
    printf("first read:  ok=%d, %lu bytes\n", ok, got);

    ok = ReadFile(read_end, buf, sizeof buf, &got, NULL);
    printf("second read: ok=%d, %lu bytes, error=%lu\n", ok, got, ok ? 0 : GetLastError());

    CloseHandle(read_end);
    return 0;
}