/* wprint.c：printf("%ls") 需要区域设置。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>

int main(void) {
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("当前区域设置：%s\n", setlocale(LC_ALL, NULL));
    int n = printf("%ls\n", L"数据 abc");
    printf("printf 返回 %d\n", n);

    const char *loc = setlocale(LC_ALL, ".UTF8");
    printf("setlocale 返回：%s\n", loc == NULL ? "(NULL)" : loc);
    n = printf("%ls\n", L"数据 abc");
    printf("printf 返回 %d\n", n);
    return 0;
}