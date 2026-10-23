/* 同一个 OVERLAPPED 同时用于两次读：两个操作都会挂起，事件的信号混在一起。 */
#include <windows.h>
#include <stdio.h>

#define BUF_SIZE (64 * 1024)

static char g_buf1[BUF_SIZE];
static char g_buf2[BUF_SIZE];

int wmain(int argc, wchar_t **argv) {
    HANDLE h;
    HANDLE ev;
    OVERLAPPED ov = {0};
    DWORD n = 0;
    BOOL ok1;
    BOOL ok2;

    if (argc != 2) {
        printf("usage: reuse_ov <file>\n");
        return 1;
    }
    h = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                    FILE_FLAG_OVERLAPPED, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: %lu\n", GetLastError());
        return 1;
    }
    ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    ov.hEvent = ev;

    ov.Offset = 0;
    ok1 = ReadFile(h, g_buf1, sizeof g_buf1, &n, &ov);
    printf("first  read at offset 0: %s", ok1 ? "TRUE" : "FALSE");
    if (!ok1) {
        printf(", error %lu", GetLastError());
    }
    printf("\n");

    ov.Offset = 81920;
    ok2 = ReadFile(h, g_buf2, sizeof g_buf2, &n, &ov);
    printf("second read at offset 81920, same OVERLAPPED: %s",
           ok2 ? "TRUE" : "FALSE");
    if (!ok2) {
        printf(", error %lu", GetLastError());
    }
    printf("\n");

    if (!ok1 || !ok2) {
        DWORD got = 0;

        WaitForSingleObject(ev, INFINITE);
        if (GetOverlappedResult(h, &ov, &got, FALSE)) {
            printf("GetOverlappedResult: TRUE, %lu bytes at offset %lu\n", got,
                   ov.Offset);
        } else {
            printf("GetOverlappedResult: FALSE, error %lu\n", GetLastError());
        }
    }
    CloseHandle(ev);
    CloseHandle(h);
    return 0;
}