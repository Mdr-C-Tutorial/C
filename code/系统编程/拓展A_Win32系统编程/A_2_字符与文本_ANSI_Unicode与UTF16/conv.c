/* conv.c：UTF-8 与 UTF-16 往返。 */
#include "textconv.h"

#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static void dump_bytes(const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p != 0; p++) {
        printf(" %02X", *p);
    }
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);

    const char *samples[] = {"abc", "数据.txt", "中文 UTF-8 输出", "\U0001F600 表情"};
    for (int i = 0; i < 4; i++) {
        wchar_t *wide = utf8_to_wide(samples[i]);
        if (wide == NULL) {
            printf("utf8_to_wide failed: %lu\n", GetLastError());
            return 1;
        }
        printf("[%s]\n  UTF-16 %d 单元：", samples[i], (int)wcslen(wide));
        for (int k = 0; wide[k] != 0; k++) {
            printf(" U+%04X", (unsigned)wide[k]);
        }
        printf("\n");

        char *back = wide_to_utf8(wide);
        if (back == NULL) {
            printf("  wide_to_utf8 failed: %lu\n", GetLastError());
            free(wide);
            return 1;
        }
        printf("  往返字节：");
        dump_bytes(back);
        printf("\n  结果：%s\n", strcmp(back, samples[i]) == 0 ? "与原文相同" : "与原文不同");
        free(back);
        free(wide);
    }
    return 0;
}