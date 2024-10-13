#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    printf("示例 - 使用 printf() - Mdr-C-Tutorial\n");

    const char c = 'M';
    printf("字符：\n");
    printf("\t%c\n", c);
    printf("\t%%\n");

    const char* s = "Mdr-C";
    printf("字符串：\n");
    printf("  填充:\n");
    printf("\t[%10s]\n", s);
    printf("\t[%-10s]\n", s);
    printf("\t[%*s]\n", 10, s);
    printf("  截断（设置精度）:\n");
    printf("\t%.4s\n", s);
    printf("\t%.*s\n", 2, s);

    int n1 = 3;
    int n2 = -42;
    int n3 = 89757;
    int n4 = 0;
    printf("整数:\n");
    printf("\t十进制:\t\t%i %d\n", n1, n2);
    printf("\t十六进制:\t%x %x %X %#x %#X\n", n1, n3, n3, n3, n3);
    printf("\t八进制:\t\t%o %o %#o\n", n1, n3, n3);

    printf("浮点:\n");
    printf("\t舍入:\t\t%f %.0f %.32f\n", 1.5, 1.5, 1.3);
    printf("\t填充:\t\t%05.2f %.2f %5.2f\n", 1.5, 1.5, 1.5);
    printf("\t科学表示:\t%E %e\n", 1.5, 1.5);
    printf("\t十六进制:\t%a %A\n", 1.5, 1.5);
    printf("\t特殊值:\t\t0/0=%g 1/0=%g\n", 0.0 / 0.0, 1.0 / 0.0);

    printf("定宽类型:\n");
    printf("\t最大的 32 位值是 %" PRIu32 " 或 %#" PRIx32 "\n", UINT32_MAX,
           UINT32_MAX);

    int n = 0;
    printf("字符数：\n");
    printf("Write characters\n%n", &n);
    printf("%d\n", n);
}
