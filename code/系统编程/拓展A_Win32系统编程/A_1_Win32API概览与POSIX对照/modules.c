/* modules.c：这条链上的模块在进程里解析成了什么。 */
#include <stdio.h>
#include <windows.h>

static void show(const wchar_t *name) {
    HMODULE m = GetModuleHandleW(name);
    wchar_t path[MAX_PATH];

    if (m == NULL) {
        m = LoadLibraryW(name);
        if (m == NULL) {
            printf("%-36ls not present, error %lu\n", name, GetLastError());
            return;
        }
    }
    DWORD n = GetModuleFileNameW(m, path, MAX_PATH);
    path[n < MAX_PATH ? n : MAX_PATH - 1] = L'\0';
    printf("%-36ls -> %ls\n", name, path);
}

int main(void) {
    show(L"kernel32.dll");
    show(L"kernelbase.dll");
    show(L"ntdll.dll");
    show(L"ucrtbase.dll");
    show(L"api-ms-win-crt-stdio-l1-1-0.dll");
    show(L"api-ms-win-core-file-l1-2-0.dll");
    return 0;
}