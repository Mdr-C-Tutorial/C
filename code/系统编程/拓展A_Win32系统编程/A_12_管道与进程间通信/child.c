/* 演示用的子进程：逐行打印收到的参数，以第一个参数作为退出码。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

int wmain(int argc, wchar_t *argv[]) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    for (int i = 1; i < argc; i++) {
        wprintf(L"argv[%d] = %ls\n", i, argv[i]);
    }
    if (argc > 1) {
        return _wtoi(argv[1]);
    }
    return 0;
}