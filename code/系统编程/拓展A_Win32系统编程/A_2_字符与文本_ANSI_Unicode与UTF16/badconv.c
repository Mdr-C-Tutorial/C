/* badconv.c：非法输入时返回值与错误码。 */
#include "textconv.h"

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static void describe(const char *tag, int ok, DWORD err) {
    printf("%-28s 返回 %s，GetLastError() = %lu\n", tag, ok ? "非 NULL" : "NULL", err);
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);

    const char *bad[] = {"\x80", "\xE6\x95"};   /* 孤立续字节；截断的三字节序列 */
    for (int i = 0; i < 2; i++) {
        SetLastError(0);
        wchar_t *wide = utf8_to_wide(bad[i]);
        char tag[64];
        snprintf(tag, sizeof tag, "非法 UTF-8 第 %d 例", i + 1);
        describe(tag, wide != NULL, GetLastError());
        free(wide);
    }

    const wchar_t lone[] = {0xD83D, 0x0021, 0};  /* 孤立的代理项 */
    SetLastError(0);
    char *back = wide_to_utf8(lone);
    describe("孤立代理项 D83D 0021", back != NULL, GetLastError());
    free(back);

    /* 同一段非法 UTF-8 交给 GBK 解释：GBK 不认为它非法，转换成功 */
    SetLastError(0);
    int n = MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, "\xE6\x95", -1, NULL, 0);
    printf("CP936 + MB_ERR_INVALID_CHARS   返回 %d，GetLastError() = %lu\n", n, GetLastError());

    /* WC_ERR_INVALID_CHARS 只能用于 UTF-8 与 GB18030 */
    SetLastError(0);
    n = WideCharToMultiByte(CP_ACP, WC_ERR_INVALID_CHARS, L"x", -1, NULL, 0, NULL, NULL);
    printf("CP_ACP + WC_ERR_INVALID_CHARS  返回 %d，GetLastError() = %lu\n", n, GetLastError());

    /* 关掉 MB_ERR_INVALID_CHARS 时，非法序列被换成 U+FFFD */
    wchar_t replaced[8] = {0};
    n = MultiByteToWideChar(CP_UTF8, 0, "\xE6\x95", -1, replaced, 8);
    printf("CP_UTF8 + flags=0              返回 %d，第一个单元 U+%04X\n", n, (unsigned)replaced[0]);
    return 0;
}