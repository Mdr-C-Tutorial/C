/*
 * Powered by Mdr-C-Tutorial. No rights reserved.
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("示例 - 使用 printf() - Mdr-C-Tutorial\n");

    const char c = 'M';
    printf("字符：\n");
    printf("\t%c\n", c);
    printf("\t%%\n");

    const char *s = "Mdr-C";
    printf("字符串：\n");
    printf("   填充:\n");
    printf("\t[%10s]\n", s);
    printf("\t[%-10s]\n", s);
    printf("\t[%*s]\n", 10, s);
    printf("   截断（设置精度）:\n");
    printf("\t%.4s\n", s);
    printf("\t%.*s\n", 2, s);

    int n1 = 3;
    int n2 = -42;
    int n3 = 89757;
    printf("整数:\n");
    printf("\tint 十进制:\t%i %d\n", n1, n2);
    printf("\tint 十六进制:\t%x %x %X %#x %#X\n", n1, n3, n3, n3, n3);
    printf("\tint 八进制:\t%o %o %#o\n", n1, n3, n3);

    signed char n5 = -50;
    short n6 = -50;
    long n7 = -50;
    long long n8 = -50;
    intmax_t n9 = -50;
    printf("\tsigned char: \t%hhd\n", n5);
    printf("\tshort:\t\t%hd\n", n6);
    printf("\tlong:\t\t%ld\n", n7);
    printf("\tlong long:\t%lld\n", n8);
    printf("\tintmax_t:\t%jd\n", n9);

    unsigned char n10 = 47;
    unsigned short n11 = 47;
    unsigned long n12 = 47;
    unsigned long long n13 = 47;
    uintmax_t n14 = 47;
    printf("\tunsigned char 十/八/十六进制：\t%hhu %hho %hhx %hhX\n", n10, n10,
           n10, n10);
    printf("\tunsigned short:\t\t%hu\n", n11);
    printf("\tunsigned long:\t\t%lu\n", n12);
    printf("\tunsigned long long:\t%llu\n", n13);
    printf("\tuintmax_t:\t%ju\n", n14);

    const char *p1 = s + 1;
    const char *p2 = s + 3;
    printf("\tptrdiff_t:\t%td %td\n", p2 - p1, p1 - p2);
    size_t size = strlen(s);
    printf("\tsize_t:\t\t%zu\n", size);

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
