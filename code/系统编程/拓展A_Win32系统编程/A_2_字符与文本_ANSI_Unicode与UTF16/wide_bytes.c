/* wide_bytes.c：同一段文字在 UTF-8 与 UTF-16 下各占多少字节、哪些码位。 */
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static void dump_utf8(const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p != 0; p++) {
        printf(" %02X", *p);
    }
}

static void dump_utf16(const wchar_t *s) {
    for (int i = 0; s[i] != 0; i++) {
        printf(" U+%04X", (unsigned)s[i]);
    }
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);

    const char *samples[] = {"abc", "数据", "中文 UTF-8", "\U0001F600"};
    for (int i = 0; i < 4; i++) {
        wchar_t wide[32];
        int units = MultiByteToWideChar(CP_UTF8, 0, samples[i], -1, wide, 32);
        printf("[%s]\n", samples[i]);
        printf("  UTF-8  %2d 字节：", (int)strlen(samples[i]));
        dump_utf8(samples[i]);
        printf("\n  UTF-16 %2d 单元：", units - 1);
        dump_utf16(wide);
        printf("\n");
    }
    printf("sizeof(wchar_t) = %d\n", (int)sizeof(wchar_t));
    return 0;
}