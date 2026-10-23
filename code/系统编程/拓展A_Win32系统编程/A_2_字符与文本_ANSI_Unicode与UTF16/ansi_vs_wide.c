/* ansi_vs_wide.c：同一段 UTF-8 字节分别交给 CreateFileA 与 CreateFileW。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>

static void print_code_points(const wchar_t *s) {
    for (int i = 0; s[i] != L'\0'; i++) {
        printf(" U+%04X", (unsigned)s[i]);
    }
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("GetACP() = %u, GetOEMCP() = %u\n", GetACP(), GetOEMCP());

    /* 源文件按 UTF-8 保存，窄字符串在内存里就是「数据.txt」的 UTF-8 字节 */
    const char *narrow_name = "数据.txt";
    const wchar_t *wide_name = L"数据.txt";

    printf("窄字符串的字节：");
    for (const unsigned char *p = (const unsigned char *)narrow_name; *p != 0; p++) {
        printf(" %02X", *p);
    }
    printf("\n");

    HANDLE h = CreateFileA(narrow_name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("CreateFileA failed: %lu\n", GetLastError());
        return 1;
    }
    CloseHandle(h);

    h = CreateFileW(wide_name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: %lu\n", GetLastError());
        return 1;
    }
    CloseHandle(h);

    printf("目录里的 .txt 文件：\n");
    WIN32_FIND_DATAW fd;
    HANDLE find = FindFirstFileW(L"*.txt", &fd);
    if (find == INVALID_HANDLE_VALUE) {
        printf("FindFirstFileW failed: %lu\n", GetLastError());
        return 1;
    }
    do {
        printf("  %ls", fd.cFileName);
        print_code_points(fd.cFileName);
        printf("\n");
    } while (FindNextFileW(find, &fd));
    FindClose(find);

    DeleteFileW(wide_name);
    DeleteFileA(narrow_name);
    return 0;
}