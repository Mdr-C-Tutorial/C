/* console_cp.c：控制台按输出代码页解释写入的字节。 */
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    setvbuf(stdout, NULL, _IONBF, 0);

    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written = 0;
    const char *line = "数据 UTF-8 输出\r\n";

    printf("GetConsoleOutputCP() = %u\n", GetConsoleOutputCP());

    /* 把 UTF-8 字节直接写进控制台句柄：控制台按当前输出代码页 936 解释 */
    WriteFile(h, line, (DWORD)strlen(line), &written, NULL);

    /* printf 走 UCRT：CRT 按区域设置把这段窄字符转成 UTF-16 再写控制台 */
    printf("printf: %s", line);

    SetConsoleOutputCP(CP_UTF8);
    printf("GetConsoleOutputCP() = %u\n", GetConsoleOutputCP());
    WriteFile(h, line, (DWORD)strlen(line), &written, NULL);

    /* WriteConsoleW 直接写 UTF-16，与输出代码页无关 */
    SetConsoleOutputCP(936);
    const wchar_t *wide = L"WriteConsoleW: 数据 UTF-8 输出\r\n";
    if (!WriteConsoleW(h, wide, (DWORD)wcslen(wide), &written, NULL)) {
        printf("WriteConsoleW failed: %lu\n", GetLastError());
    }
    return 0;
}