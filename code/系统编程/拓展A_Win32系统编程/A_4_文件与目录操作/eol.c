/* eol.c -- Win32 写出的字节原样落盘，CRT 的文本模式会把 \n 换成 \r\n。 */
#include <windows.h>
#include <stdio.h>

static void dump(const wchar_t *name) {
    HANDLE h = CreateFileW(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    unsigned char buf[32];
    DWORD got = 0;
    DWORD i;

    if (h == INVALID_HANDLE_VALUE) {
        printf("%-12ls cannot open: %lu\n", name, GetLastError());
        return;
    }
    ReadFile(h, buf, sizeof buf, &got, NULL);
    CloseHandle(h);
    printf("%-12ls", name);
    for (i = 0; i < got; i++) {
        printf(" %02X", buf[i]);
    }
    printf("   (%lu bytes)\n", got);
}

int main(void) {
    FILE *f;
    HANDLE h;
    DWORD got = 0;

    f = fopen("text.txt", "w"); /* 文本模式 */
    fputs("a\nb\n", f);
    fclose(f);

    f = fopen("binary.txt", "wb"); /* 二进制模式 */
    fputs("a\nb\n", f);
    fclose(f);

    h = CreateFileW(L"raw.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    WriteFile(h, "a\nb\n", 4, &got, NULL);
    CloseHandle(h);

    dump(L"text.txt");
    dump(L"binary.txt");
    dump(L"raw.txt");
    return 0;
}