/* envblock.c：给子进程一个自定义环境块，只含 MDR_MODE 与 SystemRoot。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

static wchar_t *build_block(void) {
    wchar_t *root = NULL;
    DWORD n = GetEnvironmentVariableW(L"SystemRoot", NULL, 0);

    if (n > 0) {
        root = malloc(n * sizeof(wchar_t));
        if (root != NULL) {
            GetEnvironmentVariableW(L"SystemRoot", root, n);
        }
    }
    const wchar_t *mode = L"MDR_MODE=test";
    size_t root_len = root != NULL ? wcslen(root) : 0;
    size_t total = wcslen(mode) + 1 + 11 + root_len + 1 + 1;
    wchar_t *block = calloc(total, sizeof(wchar_t));

    if (block == NULL) {
        free(root);
        return NULL;
    }
    wchar_t *p = block;
    memcpy(p, mode, wcslen(mode) * sizeof(wchar_t));
    p += wcslen(mode);
    *p++ = L'\0';
    memcpy(p, L"SystemRoot=", 11 * sizeof(wchar_t));
    p += 11;
    if (root != NULL) {
        memcpy(p, root, root_len * sizeof(wchar_t));
        p += root_len;
    }
    *p++ = L'\0';
    *p = L'\0';
    free(root);
    return block;
}

int wmain(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    wchar_t *block = build_block();

    if (block == NULL) {
        printf("out of memory\n");
        return 1;
    }
    wchar_t exe[MAX_PATH];
    DWORD m = GetCurrentDirectoryW(MAX_PATH, exe);

    if (m > 0) {
        _snwprintf(exe + m, MAX_PATH - m, L"\\showenv.exe");
    }
    wchar_t cmd[MAX_PATH];
    wcscpy(cmd, exe);
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;

    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_UNICODE_ENVIRONMENT, block,
                        L"C:\\Windows", &si, &pi)) {
        printf("CreateProcessW failed: %lu\n", GetLastError());
        free(block);
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    free(block);
    return 0;
}