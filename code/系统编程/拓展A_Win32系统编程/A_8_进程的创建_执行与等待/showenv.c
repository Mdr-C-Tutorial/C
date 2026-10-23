/* showenv.exe：打印几个环境变量和当前目录，供 envblock.exe 调用。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

static void show(const wchar_t *name) {
    DWORD n = GetEnvironmentVariableW(name, NULL, 0);

    if (n == 0) {
        wprintf(L"%ls = (未定义)\n", name);
        return;
    }
    wchar_t *buf = malloc(n * sizeof(wchar_t));

    if (buf == NULL) {
        return;
    }
    GetEnvironmentVariableW(name, buf, n);
    wprintf(L"%ls = %ls\n", name, buf);
    free(buf);
}

int wmain(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    show(L"MDR_MODE");
    show(L"SystemRoot");

    wchar_t cwd[MAX_PATH];
    DWORD n = GetCurrentDirectoryW(MAX_PATH, cwd);

    if (n > 0) {
        wprintf(L"cwd = %ls\n", cwd);
    }
    wprintf(L"PATH is %ls\n",
            GetEnvironmentVariableW(L"PATH", NULL, 0) > 0 ? L"defined" : L"missing");
    return 0;
}