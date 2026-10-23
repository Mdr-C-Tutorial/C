/* share_demo.c -- 共享模式决定别人能不能读写、能不能删除；兼容性检查是双向的。 */
#include <windows.h>
#include <stdio.h>

static void try_delete(const wchar_t *name) {
    BOOL ok = DeleteFileW(name);

    printf("  DeleteFileW: ok=%d err=%lu\n", ok, ok ? 0ul : GetLastError());
}

static void try_open(const wchar_t *name, DWORD access, DWORD share,
                     const char *want) {
    HANDLE h = CreateFileW(name, access, share, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);

    printf("  open for %s, share 0x%lx: %-6s error %lu\n", want,
           (unsigned long)share, h == INVALID_HANDLE_VALUE ? "failed" : "ok",
           h == INVALID_HANDLE_VALUE ? GetLastError() : 0ul);
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
    }
}

int main(void) {
    HANDLE h;
    DWORD got = 0;

    printf("== holder: GENERIC_READ|GENERIC_WRITE, share 0 ==\n");
    h = CreateFileW(L"log0.txt", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    WriteFile(h, "line\n", 5, &got, NULL);
    try_delete(L"log0.txt");
    try_open(L"log0.txt", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, "read");
    CloseHandle(h);
    printf("  after the handle is closed:\n");
    try_delete(L"log0.txt");

    printf("== holder: GENERIC_READ|GENERIC_WRITE, share READ|WRITE ==\n");
    h = CreateFileW(L"log1.txt", GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    WriteFile(h, "line\n", 5, &got, NULL);
    try_delete(L"log1.txt");
    try_open(L"log1.txt", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, "read");
    CloseHandle(h);
    DeleteFileW(L"log1.txt");

    printf("== holder: GENERIC_WRITE, share READ|DELETE (a log writer) ==\n");
    h = CreateFileW(L"log2.txt", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE,
                    NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    WriteFile(h, "line\n", 5, &got, NULL);
    try_open(L"log2.txt", GENERIC_READ, FILE_SHARE_READ, "read");
    try_open(L"log2.txt", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, "read");
    try_delete(L"log2.txt");
    CloseHandle(h);

    printf("== holder: GENERIC_READ|GENERIC_WRITE, share READ|DELETE ==\n");
    h = CreateFileW(L"log3.txt", GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    WriteFile(h, "line\n", 5, &got, NULL);
    try_delete(L"log3.txt");
    printf("  GetFileAttributesW: 0x%lx\n",
           (unsigned long)GetFileAttributesW(L"log3.txt"));
    try_open(L"log3.txt", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, "read");
    {
        LARGE_INTEGER zero = {.QuadPart = 0};
        char buf[16] = {0};
        BOOL ok;

        SetFilePointerEx(h, zero, NULL, FILE_BEGIN);
        ok = ReadFile(h, buf, sizeof buf - 1, &got, NULL);
        printf("  read through the surviving handle: ok=%d, %lu bytes\n", ok,
               got);
    }
    CloseHandle(h);
    return 0;
}