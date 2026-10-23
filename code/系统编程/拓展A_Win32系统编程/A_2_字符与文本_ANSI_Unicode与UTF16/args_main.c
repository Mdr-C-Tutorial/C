/* args_main.c：同样的命令行用普通 main 接收。 */
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

int main(int argc, char **argv) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d]:", i);
        for (const unsigned char *p = (const unsigned char *)argv[i]; *p != 0; p++) {
            printf(" %02X", *p);
        }
        printf(" (%d 字节)\n", (int)strlen(argv[i]));
    }
    return 0;
}