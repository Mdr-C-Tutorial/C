/* 完成例程只在线程进入可警报等待时运行：Sleep 期间不运行，SleepEx(..., TRUE) 期间运行。 */
#include <windows.h>
#include <stdio.h>

#define BUF_SIZE (64 * 1024)

static char g_buf1[BUF_SIZE];
static char g_buf2[BUF_SIZE];
static int g_runs;

static void CALLBACK on_read(DWORD err, DWORD bytes, LPOVERLAPPED ov) {
    (void)ov;
    g_runs++;
    printf("  completion routine ran: %lu bytes, error %lu\n", bytes, err);
    fflush(stdout);
}

int wmain(int argc, wchar_t **argv) {
    HANDLE h;
    OVERLAPPED ov1 = {0};
    OVERLAPPED ov2 = {0};
    DWORD rc;

    if (argc != 2) {
        printf("usage: no_alert <file>\n");
        return 1;
    }
    h = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                    FILE_FLAG_OVERLAPPED, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: %lu\n", GetLastError());
        return 1;
    }

    if (!ReadFileEx(h, g_buf1, BUF_SIZE, &ov1, on_read)) {
        printf("ReadFileEx failed: %lu\n", GetLastError());
        CloseHandle(h);
        return 1;
    }
    printf("first read issued, now Sleep(500), a non-alertable wait\n");
    Sleep(500);
    printf("after Sleep: completion routines run = %d\n", g_runs);

    if (!ReadFileEx(h, g_buf2, BUF_SIZE, &ov2, on_read)) {
        printf("ReadFileEx failed: %lu\n", GetLastError());
        CloseHandle(h);
        return 1;
    }
    printf("second read issued, now SleepEx(500, TRUE)\n");
    rc = SleepEx(500, TRUE);
    printf("SleepEx returned %lu (%s)\n", rc,
           rc == WAIT_IO_COMPLETION ? "WAIT_IO_COMPLETION" : "timeout");
    printf("after SleepEx: completion routines run = %d\n", g_runs);

    CloseHandle(h);
    return 0;
}