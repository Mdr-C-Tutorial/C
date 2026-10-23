/* wargs.c：用 wmain 取得完整的 Unicode 命令行。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>

int wmain(int argc, wchar_t **argv) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = [%ls] %d 个代码单元：", i, argv[i], (int)wcslen(argv[i]));
        for (int k = 0; argv[i][k] != 0; k++) {
            printf(" U+%04X", (unsigned)argv[i][k]);
        }
        printf("\n");
    }
    return 0;
}