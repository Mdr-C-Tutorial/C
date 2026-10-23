/* cpw.c：用 Win32 的文件 API 复制一个文件。 */
#include <stdio.h>
#include <windows.h>

#define BUF_SIZE 65536

int wmain(int argc, wchar_t *argv[]) {
    if (argc != 3) {
        printf("usage: cpw source destination\n");
        return 2;
    }

    HANDLE in = INVALID_HANDLE_VALUE;
    HANDLE out = INVALID_HANDLE_VALUE;
    static char buf[BUF_SIZE];
    unsigned long long total = 0;
    int rc = 1;

    in = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL, NULL);
    if (in == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: error %lu\n", GetLastError());
        goto cleanup;
    }

    out = CreateFileW(argv[2], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL, NULL);
    if (out == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: error %lu\n", GetLastError());
        goto cleanup;
    }

    for (;;) {
        DWORD got = 0;
        if (!ReadFile(in, buf, sizeof buf, &got, NULL)) {
            printf("ReadFile failed: error %lu\n", GetLastError());
            goto cleanup;
        }
        if (got == 0) {
            break;
        }
        DWORD put = 0;
        if (!WriteFile(out, buf, got, &put, NULL) || put != got) {
            printf("WriteFile failed: error %lu\n", GetLastError());
            goto cleanup;
        }
        total += got;
    }

    printf("copied %llu bytes\n", total);
    rc = 0;

cleanup:
    if (out != INVALID_HANDLE_VALUE) {
        CloseHandle(out);
    }
    if (in != INVALID_HANDLE_VALUE) {
        CloseHandle(in);
    }
    return rc;
}