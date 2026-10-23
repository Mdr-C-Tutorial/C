/* vt_color.c —— 用虚拟终端序列输出颜色、控制光标。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -o vt_color.exe vt_color.c
 * 运行：vt_color.exe          （控制台里看颜色）
 *       vt_color.exe > out.txt（重定向时得到的是转义序列字节）
 */
#include <windows.h>
#include <stdio.h>

/* 开启 VT 处理。stdout 不是控制台时什么也不做：序列照样写出去，
 * 重定向到文件或管道时得到的就是那段字节。 */
static int enable_vt(HANDLE out, DWORD *saved) {
    if (!GetConsoleMode(out, saved)) {
        *saved = 0;
        return 0;
    }
    if (!SetConsoleMode(out, *saved | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        printf("SetConsoleMode failed: %lu\n", GetLastError());
        return -1;
    }
    return 1;
}

int main(void) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD saved;
    int is_console = enable_vt(out, &saved);
    if (is_console < 0) {
        return 1;
    }

    printf("\x1b[1mforeground 30-37 / 90-97\x1b[0m\n");
    for (int i = 0; i < 8; i++) {
        printf("\x1b[%dm %d \x1b[0m", 30 + i, i);
    }
    printf("\n");
    for (int i = 0; i < 8; i++) {
        printf("\x1b[%dm %d \x1b[0m", 90 + i, i + 8);
    }
    printf("\n\n");

    printf("\x1b[1mbackground 40-47 and 24-bit color\x1b[0m\n");
    for (int i = 0; i < 8; i++) {
        printf("\x1b[%dm   \x1b[0m", 40 + i);
    }
    printf("  \x1b[48;2;200;60;20m   \x1b[0m \x1b[38;2;30;144;255mtruecolor\x1b[0m\n\n");

    printf("\x1b[1mredrawn in place\x1b[0m\n");
    printf("\x1b[?25l"); /* 隐藏光标 */
    for (int step = 0; step <= 3; step++) {
        printf("\x1b[2K\rstep %d of 3  ", step); /* 清整行，回到行首 */
        fflush(stdout);
        Sleep(250);
    }
    printf("\x1b[2K\rstep 3 of 3  done\n");
    printf("\x1b[?25h"); /* 恢复光标 */

    if (is_console) {
        SetConsoleMode(out, saved);
    }
    return 0;
}