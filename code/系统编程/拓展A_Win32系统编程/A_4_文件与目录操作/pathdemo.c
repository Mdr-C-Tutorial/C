/* pathdemo.c -- 路径的几种写法、大小写、规范化与 MAX_PATH 限制。 */
#include <windows.h>
#include <stdio.h>
#include <locale.h>

static void show_full_path(const wchar_t *in) {
    wchar_t out[MAX_PATH];
    DWORD n = GetFullPathNameW(in, MAX_PATH, out, NULL);

    printf("%-26ls -> %ls (len %lu)\n", in, n ? out : L"(failed)", n);
}

int main(void) {
    wchar_t cwd[MAX_PATH];
    wchar_t dir[MAX_PATH * 2];
    wchar_t file[MAX_PATH * 2];
    wchar_t ext[MAX_PATH * 2 + 8];
    HANDLE h;
    DWORD got = 0;
    int i;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    /* 正斜杠、. 与 ..、UNC，都被同一个函数规范化 */
    show_full_path(L"C:\\work\\sub\\..\\a.txt");
    show_full_path(L"C:/x/y.txt");
    show_full_path(L"\\\\server\\share\\dir\\f.txt");

    /* 文件名不区分大小写：先建 CaseTest.TXT，再用小写打开 */
    h = CreateFileW(L"CaseTest.TXT", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    CloseHandle(h);
    h = CreateFileW(L"casetest.txt", GENERIC_READ, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, 0, NULL);
    printf("open casetest.txt while CaseTest.TXT exists: %d\n",
           h != INVALID_HANDLE_VALUE);
    CloseHandle(h);
    DeleteFileW(L"CaseTest.TXT");

    printf("CompareStringOrdinal ignores case: %d (CSTR_EQUAL is %d)\n",
           CompareStringOrdinal(L"abc", -1, L"ABC", -1, TRUE), CSTR_EQUAL);

    /* 造一条超过 MAX_PATH 的路径：逐层创建时必须加 \\?\ 前缀 */
    GetCurrentDirectoryW(MAX_PATH, cwd);
    _snwprintf(dir, MAX_PATH * 2, L"%ls\\deeppath", cwd);
    CreateDirectoryW(dir, NULL);
    for (i = 0; i < 16; i++) {
        wchar_t name[32];

        _snwprintf(name, 32, L"\\abcdefghij%02d", i);
        wcscat(dir, name);
        _snwprintf(ext, MAX_PATH * 2 + 8, L"\\\\?\\%ls", dir);
        CreateDirectoryW(ext, NULL);
    }
    _snwprintf(file, MAX_PATH * 2, L"%ls\\file.txt", dir);
    printf("long path length %d, MAX_PATH is %d (length depends on the current "
           "directory)\n",
           (int)wcslen(file), MAX_PATH);

    h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    printf("open without prefix: %d, error %lu\n", h != INVALID_HANDLE_VALUE,
           h == INVALID_HANDLE_VALUE ? GetLastError() : 0ul);
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
    }

    _snwprintf(ext, MAX_PATH * 2 + 8, L"\\\\?\\%ls", file);
    h = CreateFileW(ext, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    printf("open with prefix: %d, error %lu\n", h != INVALID_HANDLE_VALUE,
           h == INVALID_HANDLE_VALUE ? GetLastError() : 0ul);
    if (h != INVALID_HANDLE_VALUE) {
        WriteFile(h, "x", 1, &got, NULL);
        CloseHandle(h);
        DeleteFileW(ext);
    }
    return 0;
}