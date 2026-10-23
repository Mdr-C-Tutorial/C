/* 重叠 I/O 的三种结果：同步完成、ERROR_IO_PENDING、读到文件末尾的 ERROR_HANDLE_EOF。 */
#include <windows.h>
#include <stdio.h>

int wmain(int argc, wchar_t **argv) {
    HANDLE h;
    HANDLE ev;
    OVERLAPPED ov = {0};
    LARGE_INTEGER size;
    char buf[4096];
    DWORD n = 0;
    DWORD err;
    BOOL ok;

    if (argc != 2) {
        printf("usage: ov_basic <file>\n");
        return 1;
    }
    h = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                    FILE_FLAG_OVERLAPPED, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: %lu\n", GetLastError());
        return 1;
    }
    if (!GetFileSizeEx(h, &size)) {
        printf("GetFileSizeEx failed: %lu\n", GetLastError());
        CloseHandle(h);
        return 1;
    }
    ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (ev == NULL) {
        printf("CreateEventW failed: %lu\n", GetLastError());
        CloseHandle(h);
        return 1;
    }
    ov.hEvent = ev;
    printf("file size: %lld bytes\n", (long long)size.QuadPart);

    ok = ReadFile(h, buf, sizeof buf, &n, &ov);
    err = ok ? 0 : GetLastError();
    printf("read at offset 0: ");
    if (ok) {
        printf("TRUE, %lu bytes already in the buffer\n", n);
    } else if (err == ERROR_IO_PENDING) {
        printf("FALSE, error %lu (ERROR_IO_PENDING)\n", err);
        WaitForSingleObject(ev, INFINITE);
        ok = GetOverlappedResult(h, &ov, &n, FALSE);
        if (ok) {
            printf("after the event: %lu bytes\n", n);
        } else {
            printf("after the event: error %lu\n", GetLastError());
        }
    } else {
        printf("FALSE, error %lu\n", err);
    }
    if (!ok) {
        CloseHandle(ev);
        CloseHandle(h);
        return 1;
    }

    ov.Offset = size.LowPart;
    ov.OffsetHigh = size.HighPart;
    n = 0;
    ok = ReadFile(h, buf, sizeof buf, &n, &ov);
    err = ok ? 0 : GetLastError();
    printf("read starting at offset %lld (the end of the file): ",
           (long long)size.QuadPart);
    if (ok) {
        printf("TRUE, %lu bytes\n", n);
    } else if (err == ERROR_IO_PENDING) {
        printf("FALSE, error %lu\n", err);
        WaitForSingleObject(ev, INFINITE);
        if (GetOverlappedResult(h, &ov, &n, FALSE)) {
            printf("after the event: %lu bytes\n", n);
        } else {
            printf("after the event: error %lu (ERROR_HANDLE_EOF)\n",
                   GetLastError());
        }
    } else {
        printf("FALSE, error %lu\n", err);
    }

    CloseHandle(ev);
    CloseHandle(h);
    return 0;
}