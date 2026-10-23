/* 同一个文件的 4 块同时发起重叠读，用 WaitForMultipleObjects 按完成顺序取回。 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define CHUNKS 4

int wmain(int argc, wchar_t **argv) {
    static const int issue_order[CHUNKS] = {2, 0, 3, 1};
    HANDLE h;
    HANDLE events[CHUNKS];
    OVERLAPPED ov[CHUNKS];
    char *buf[CHUNKS];
    LARGE_INTEGER size;
    LONGLONG total;
    LONGLONG chunk;
    int pending;
    int i;
    int completed = 0;
    int rc = 1;
    DWORD wait_rc;

    for (i = 0; i < CHUNKS; i++) {
        events[i] = NULL;
        buf[i] = NULL;
    }
    if (argc != 2) {
        printf("usage: ov_chunks <file>\n");
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
    total = size.QuadPart;
    chunk = (total + CHUNKS - 1) / CHUNKS;
    printf("file size: %lld bytes, %lld bytes per chunk\n", (long long)total,
           (long long)chunk);

    for (i = 0; i < CHUNKS; i++) {
        buf[i] = malloc((size_t)chunk);
        events[i] = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (buf[i] == NULL || events[i] == NULL) {
            printf("out of memory or CreateEventW failed\n");
            goto cleanup;
        }
    }

    pending = 0;
    for (i = 0; i < CHUNKS; i++) {
        int k = issue_order[i];
        LONGLONG off = chunk * k;
        DWORD want;
        DWORD n = 0;
        DWORD err;

        if (off >= total) {
            printf("chunk %d: offset %lld is past the end, skipped\n", k,
                   (long long)off);
            continue;
        }
        want = (DWORD)((total - off < chunk) ? total - off : chunk);
        ov[k].Offset = (DWORD)off;
        ov[k].OffsetHigh = (DWORD)(off >> 32);
        ov[k].hEvent = events[k];
        printf("issue  chunk %d: offset %lld, %lu bytes\n", k, (long long)off,
               want);
        if (ReadFile(h, buf[k], want, &n, &ov[k])) {
            printf("chunk %d completed inside ReadFile, %lu bytes\n", k, n);
            SetEvent(events[k]);
        } else {
            err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                printf("chunk %d: ReadFile failed, error %lu\n", k, err);
                goto cleanup;
            }
        }
        pending++;
    }

    while (pending > 0) {
        DWORD n = 0;
        BOOL ok;

        wait_rc = WaitForMultipleObjects(CHUNKS, events, FALSE, INFINITE);
        if (wait_rc == WAIT_FAILED || wait_rc >= WAIT_OBJECT_0 + CHUNKS) {
            printf("WaitForMultipleObjects failed: %lu\n", GetLastError());
            goto cleanup;
        }
        i = (int)(wait_rc - WAIT_OBJECT_0);
        completed++;
        ok = GetOverlappedResult(h, &ov[i], &n, FALSE);
        printf("done #%d: chunk %d (offset %lld) ", completed, i,
               (long long)chunk * i);
        if (ok) {
            printf("%lu bytes, starts with '%c'\n", n, buf[i][0]);
        } else {
            printf("error %lu\n", GetLastError());
        }
        ResetEvent(events[i]);
        pending--;
    }
    rc = 0;

cleanup:
    for (i = 0; i < CHUNKS; i++) {
        free(buf[i]);
        if (events[i] != NULL) {
            CloseHandle(events[i]);
        }
    }
    CloseHandle(h);
    return rc;
}