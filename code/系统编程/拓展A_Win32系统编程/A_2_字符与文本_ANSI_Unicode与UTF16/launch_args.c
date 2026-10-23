/* launch_args.c：用 CreateProcessW 以固定命令行启动两个被测程序，排除 shell 的干扰。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>

static void run(const wchar_t *exe, wchar_t *cmdline) {
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;
    printf("命令行：%ls\n", cmdline);
    fflush(stdout);
    if (!CreateProcessW(exe, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcessW failed: %lu\n", GetLastError());
        return;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    printf("----\n");
    fflush(stdout);
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);

    wchar_t cmd_main[] = L"args_main.exe 中文 \"a b\" \U0001F600";
    wchar_t cmd_wide[] = L"wargs.exe 中文 \"a b\" \U0001F600";

    printf("=== main 版\n");
    run(L"args_main.exe", cmd_main);
    printf("=== wmain 版\n");
    run(L"wargs.exe", cmd_wide);
    return 0;
}