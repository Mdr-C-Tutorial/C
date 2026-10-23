/* mojibake.c：把 UTF-8 字节按 CP936 解码后用 WriteConsoleW 原样显示。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static void show(const char *utf8) {
    int need = MultiByteToWideChar(936, 0, utf8, -1, NULL, 0);
    wchar_t *wide = malloc((size_t)need * sizeof *wide);
    if (wide == NULL) {
        return;
    }
    MultiByteToWideChar(936, 0, utf8, -1, wide, need);

    printf("原文 [%s] -> CP936: [", utf8);
    fflush(stdout);
    DWORD written;
    WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), wide, (DWORD)wcslen(wide), &written, NULL);
    printf("]  码位：");
    for (int i = 0; wide[i] != 0; i++) {
        printf(" U+%04X", (unsigned)wide[i]);
    }
    printf("\n");
    free(wide);
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);

    show("数据.txt");
    show("数据");
    show("输出");
    return 0;
}