/* 内存映射文件：三种方式统计一个文件的行数，mapcount.exe <file> */
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define READ_CHUNK (1u << 20)    /* ReadFile 版本每次读 1 MiB */
#define WINDOW ((SIZE_T)4 << 20) /* 分段映射的窗口，4 MiB */

static void print_error(const wchar_t *what, DWORD err) {
    wchar_t *msg = NULL;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                 FORMAT_MESSAGE_FROM_SYSTEM |
                                 FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL, err, 0, (wchar_t *)&msg, 0, NULL);
    while (n > 0 && (msg[n - 1] == L'\r' || msg[n - 1] == L'\n')) {
        msg[--n] = L'\0';
    }
    wprintf(L"%ls: error %lu: %ls\n", what, (unsigned long)err,
            n > 0 ? msg : L"(no message)");
    LocalFree(msg);
}

static unsigned long long count_newlines(const char *p, size_t n) {
    unsigned long long lines = 0;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == '\n') {
            lines++;
        }
    }
    return lines;
}

/* 方式一：ReadFile 循环 */
static int count_with_readfile(const wchar_t *path, unsigned long long *lines,
                               unsigned long long *bytes) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        print_error(L"CreateFileW", GetLastError());
        return -1;
    }
    char *buf = malloc(READ_CHUNK);
    if (buf == NULL) {
        CloseHandle(h);
        return -1;
    }

    *lines = 0;
    *bytes = 0;
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(h, buf, READ_CHUNK, &got, NULL)) {
            print_error(L"ReadFile", GetLastError());
            free(buf);
            CloseHandle(h);
            return -1;
        }
        if (got == 0) {
            break;
        }
        *bytes += got;
        *lines += count_newlines(buf, got);
    }
    free(buf);
    CloseHandle(h);
    return 0;
}

/* 方式二：整个文件一个视图 */
static int count_with_view(const wchar_t *path, unsigned long long *lines,
                           unsigned long long *bytes) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        print_error(L"CreateFileW", GetLastError());
        return -1;
    }
    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size)) {
        print_error(L"GetFileSizeEx", GetLastError());
        CloseHandle(h);
        return -1;
    }
    if (size.QuadPart == 0) {
        /* 长度为 0 的文件没有可映射的内容 */
        *lines = 0;
        *bytes = 0;
        CloseHandle(h);
        return 0;
    }
    HANDLE map = CreateFileMappingW(h, NULL, PAGE_READONLY, 0, 0, NULL);
    if (map == NULL) {
        print_error(L"CreateFileMappingW", GetLastError());
        CloseHandle(h);
        return -1;
    }
    char *view = MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
    if (view == NULL) {
        print_error(L"MapViewOfFile", GetLastError());
        CloseHandle(map);
        CloseHandle(h);
        return -1;
    }

    /* 映射对象自己持有文件的引用，文件句柄现在就可以关掉 */
    CloseHandle(h);

    *bytes = (unsigned long long)size.QuadPart;
    *lines = count_newlines(view, (size_t)size.QuadPart);

    UnmapViewOfFile(view);
    CloseHandle(map);
    return 0;
}

/* 方式三：按 4 MiB 的窗口分段映射 */
static int count_with_windows(const wchar_t *path, unsigned long long *lines,
                              unsigned long long *bytes, int *windows) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        print_error(L"CreateFileW", GetLastError());
        return -1;
    }
    LARGE_INTEGER size;
    GetFileSizeEx(h, &size);
    HANDLE map = CreateFileMappingW(h, NULL, PAGE_READONLY, 0, 0, NULL);
    if (map == NULL) {
        print_error(L"CreateFileMappingW", GetLastError());
        CloseHandle(h);
        return -1;
    }

    *lines = 0;
    *bytes = 0;
    *windows = 0;
    for (unsigned long long off = 0; off < (unsigned long long)size.QuadPart;
         off += WINDOW) {
        SIZE_T len = (SIZE_T)((unsigned long long)size.QuadPart - off);
        if (len > WINDOW) {
            len = WINDOW;
        }
        char *view = MapViewOfFile(map, FILE_MAP_READ, (DWORD)(off >> 32),
                                   (DWORD)(off & 0xFFFFFFFFu), len);
        if (view == NULL) {
            print_error(L"MapViewOfFile", GetLastError());
            CloseHandle(map);
            CloseHandle(h);
            return -1;
        }
        *lines += count_newlines(view, len);
        *bytes += len;
        (*windows)++;
        UnmapViewOfFile(view);
    }
    CloseHandle(map);
    CloseHandle(h);
    return 0;
}

int wmain(int argc, wchar_t **argv) {
    if (argc != 2) {
        wprintf(L"usage: mapcount <file>\n");
        return 2;
    }
    unsigned long long lines = 0;
    unsigned long long bytes = 0;
    int windows = 0;

    if (count_with_readfile(argv[1], &lines, &bytes) == 0) {
        wprintf(L"ReadFile loop : %llu bytes, %llu lines\n", bytes, lines);
    }
    if (count_with_view(argv[1], &lines, &bytes) == 0) {
        wprintf(L"one view      : %llu bytes, %llu lines\n", bytes, lines);
    }
    if (count_with_windows(argv[1], &lines, &bytes, &windows) == 0) {
        wprintf(L"4 MiB windows : %llu bytes, %llu lines in %d windows\n", bytes,
                lines, windows);
    }

    /* 视图的文件偏移必须是分配粒度的倍数，页大小不够 */
    HANDLE h = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, 0, NULL);
    HANDLE map = CreateFileMappingW(h, NULL, PAGE_READONLY, 0, 0, NULL);
    char *bad = MapViewOfFile(map, FILE_MAP_READ, 0, 4096, 4096);
    wprintf(L"view at offset 4096: %s, error %lu\n",
            bad == NULL ? L"NULL" : L"ok", (unsigned long)GetLastError());
    if (bad != NULL) {
        UnmapViewOfFile(bad);
    }
    CloseHandle(map);
    CloseHandle(h);
    return 0;
}