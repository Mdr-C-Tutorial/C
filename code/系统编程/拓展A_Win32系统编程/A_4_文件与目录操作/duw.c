/* duw.c -- 递归统计目录：文件数、目录数与总字节数（mini du）。 */
#include <windows.h>
#include <stdio.h>
#include <locale.h>

typedef struct {
    unsigned long long files;
    unsigned long long dirs;
    unsigned long long bytes;
} totals;

static void walk(const wchar_t *dir, totals *t) {
    wchar_t pattern[MAX_PATH];
    wchar_t path[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE h;

    _snwprintf(pattern, MAX_PATH, L"%ls\\*", dir);
    /* FindExInfoBasic 不取短文件名，FIND_FIRST_EX_LARGE_FETCH 用更大的缓冲区 */
    h = FindFirstFileExW(pattern, FindExInfoBasic, &fd, FindExSearchNameMatch, NULL,
                         FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        ULARGE_INTEGER size;

        /* 每个目录都先返回 . 和 ..，递归时会绕回自己，必须跳过 */
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
            continue;
        }
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            t->dirs++;
            _snwprintf(path, MAX_PATH, L"%ls\\%ls", dir, fd.cFileName);
            walk(path, t);
            continue;
        }
        size.LowPart = fd.nFileSizeLow;
        size.HighPart = fd.nFileSizeHigh;
        t->files++;
        t->bytes += size.QuadPart;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

int wmain(int argc, wchar_t **argv) {
    const wchar_t *root = argc > 1 ? argv[1] : L".";
    totals t = {0};

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    walk(root, &t);
    printf("%llu files, %llu dirs, %llu bytes\n", t.files, t.dirs, t.bytes);
    return 0;
}