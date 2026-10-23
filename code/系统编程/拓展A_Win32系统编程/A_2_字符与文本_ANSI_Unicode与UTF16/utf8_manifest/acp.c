/* utf8_manifest/acp.c：查看进程代码页，并用窄字符 API 创建中文名文件。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("GetACP()             = %u\n", GetACP());
    printf("GetOEMCP()           = %u\n", GetOEMCP());
    printf("GetConsoleOutputCP() = %u\n", GetConsoleOutputCP());

    HANDLE h = CreateFileA("数据.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("CreateFileA failed: %lu\n", GetLastError());
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
        for (int i = 0; fd.cFileName[i] != 0; i++) {
            printf(" U+%04X", (unsigned)fd.cFileName[i]);
        }
        printf("\n");
    } while (FindNextFileW(find, &fd));
    FindClose(find);
    return 0;
}