/* cargv.c：GetCommandLineW + CommandLineToArgvW。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);

    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == NULL) {
        printf("CommandLineToArgvW failed: %lu\n", GetLastError());
        return 1;
    }
    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = [%ls]", i, argv[i]);
        for (int k = 0; argv[i][k] != 0; k++) {
            printf(" U+%04X", (unsigned)argv[i][k]);
        }
        printf("\n");
    }
    LocalFree(argv);
    return 0;
}