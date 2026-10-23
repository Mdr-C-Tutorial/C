/* atomic_save.c -- 安全地保存配置文件：临时文件 + FlushFileBuffers + MoveFileExW。 */
#include <windows.h>
#include <stdio.h>
#include <locale.h>

static BOOL write_all(HANDLE h, const char *data, DWORD len) {
    while (len > 0) {
        DWORD written = 0;

        if (!WriteFile(h, data, len, &written, NULL)) {
            return FALSE;
        }
        data += written;
        len -= written;
    }
    return TRUE;
}

/* 把 data 原子地写到 path：任何一步失败，磁盘上的原文件都保持不变。 */
static BOOL atomic_save(const wchar_t *path, const char *data, DWORD len) {
    wchar_t tmp[MAX_PATH];
    HANDLE h = INVALID_HANDLE_VALUE;
    BOOL ok = FALSE;

    /* 临时文件必须和目标在同一个目录（同一卷），替换才是原子的 */
    _snwprintf(tmp, MAX_PATH, L"%ls.tmp%lu", path, GetCurrentProcessId());

    h = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("CreateFileW(%ls) failed: %lu\n", tmp, GetLastError());
        return FALSE;
    }
    if (!write_all(h, data, len)) {
        printf("WriteFile failed: %lu\n", GetLastError());
        goto cleanup;
    }
    /* 先把数据写到磁盘，再替换名字；否则替换完成后掉电会留下空文件 */
    if (!FlushFileBuffers(h)) {
        printf("FlushFileBuffers failed: %lu\n", GetLastError());
        goto cleanup;
    }
    CloseHandle(h);
    h = INVALID_HANDLE_VALUE;

    /* 同一卷内的改名是一个原子操作：别的进程要么看到旧文件，要么看到新文件 */
    if (!MoveFileExW(tmp, path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        printf("MoveFileExW failed: %lu\n", GetLastError());
        goto cleanup;
    }
    ok = TRUE;

cleanup:
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
    }
    if (!ok) {
        DeleteFileW(tmp);
    }
    return ok;
}

static void dump(const wchar_t *path) {
    HANDLE h;
    char buf[256];
    DWORD got = 0;

    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("  %ls: cannot open, error %lu\n", path, GetLastError());
        return;
    }
    if (ReadFile(h, buf, sizeof buf - 1, &got, NULL)) {
        buf[got] = '\0';
        printf("  %ls: %lu bytes\n", path, got);
        fputs(buf, stdout);
    }
    CloseHandle(h);
}

int main(void) {
    WIN32_FIND_DATAW fd;
    HANDLE h;
    HANDLE f;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    DeleteFileW(L"app.cfg");

    printf("first save:\n");
    if (atomic_save(L"app.cfg", "mode = fast\nretries = 3\n", 24)) {
        dump(L"app.cfg");
    }

    printf("\nsecond save replaces the file:\n");
    if (atomic_save(L"app.cfg", "mode = safe\nretries = 5\nlevel = 2\n", 35)) {
        dump(L"app.cfg");
    }

    /* 对照：直接覆盖写，在写之前文件就已经被清空 */
    printf("\nplain overwrite, CREATE_ALWAYS and no write yet:\n");
    h = CreateFileW(L"app.cfg", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
    }
    dump(L"app.cfg");

    printf("\nfiles left in the directory:\n");
    f = FindFirstFileW(L"app.cfg*", &fd);
    if (f != INVALID_HANDLE_VALUE) {
        do {
            printf("  %-16ls %lu bytes\n", fd.cFileName, fd.nFileSizeLow);
        } while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    return 0;
}