/* rules_demo.c：把几条写死的命令行原样交给 child.exe，看 CRT 切出的 argv。 */
#include <locale.h>
#include <stdio.h>
#include <wchar.h>
#include <windows.h>

static void run(const wchar_t *label, const wchar_t *line) {
    wchar_t cmd[1024];

    _snwprintf(cmd, 1024, L"child.exe %ls", line);
    wprintf(L"--- %ls\n    child.exe %ls\n", label, line);
    fflush(stdout);
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;

    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcessW failed: %lu\n", GetLastError());
        return;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    run(L"引号内的空白", L"0 \"a b c\" d e");
    run(L"转义引号与单个反斜杠", L"0 \"ab\\\"c\" \"\\\\\" d");
    run(L"三反斜杠加 b", L"0 a\\\\\\b d\"e f\"g h");
    run(L"反斜杠转义引号", L"0 a\\\\\\\"b c d");
    run(L"四个反斜杠", L"0 a\\\\\\\\\"b c\" d e");
    run(L"结尾反斜杠吃掉后一个参数", L"0 \"C:\\dir with space\\\" tail");
    run(L"反斜杠成对保住参数", L"0 \"C:\\dir with space\\\\\" tail");
    return 0;
}