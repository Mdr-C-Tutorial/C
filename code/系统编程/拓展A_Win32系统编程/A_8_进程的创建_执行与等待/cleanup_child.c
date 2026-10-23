/* cleanup_child.c：注册 atexit 处理程序后睡若干秒。
 * 正常退出会写出最后一行，被 TerminateProcess 结束则不会。 */
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static void on_exit_handler(void) {
    printf("cleanup ran\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    atexit(on_exit_handler);
    unsigned seconds = argc > 1 ? (unsigned)atoi(argv[1]) : 30;

    fflush(stdout);
    Sleep(seconds * 1000);
    return 0;
}