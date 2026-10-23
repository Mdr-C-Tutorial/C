/* 重叠句柄不维护文件位置：SetFilePointerEx 移动的位置不影响重叠读，
   读的起点由 OVERLAPPED 的 Offset 决定。 */
#include <windows.h>
#include <stdio.h>

int wmain(int argc, wchar_t **argv) {
    HANDLE h;
    OVERLAPPED ov = {0};
    LARGE_INTEGER pos;
    char buf[16];
    DWORD n = 0;

    if (argc != 2) {
        printf("usage: offset_demo <file>\n");
        return 1;
    }
    h = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                    FILE_FLAG_OVERLAPPED, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: %lu\n", GetLastError());
        return 1;
    }
    pos.QuadPart = 10;
    SetFilePointerEx(h, pos, NULL, FILE_BEGIN);
    printf("SetFilePointerEx moved the file pointer to 10\n");

    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    ov.Offset = 0;
    if (!ReadFile(h, buf, 4, &n, &ov) && GetLastError() == ERROR_IO_PENDING) {
        WaitForSingleObject(ov.hEvent, INFINITE);
        GetOverlappedResult(h, &ov, &n, FALSE);
    }
    printf("overlapped read with Offset = 0 returned %lu bytes: %c%c%c%c\n", n,
           buf[0], buf[1], buf[2], buf[3]);
    CloseHandle(ov.hEvent);
    CloseHandle(h);
    return 0;
}