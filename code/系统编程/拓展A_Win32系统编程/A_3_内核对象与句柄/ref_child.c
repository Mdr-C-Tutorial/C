/* ref_child.c：继承父进程的命名事件句柄，持有一秒半后关闭并退出。 */
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

int main(int argc, char *argv[]) {
    HANDLE event;
    DWORD flags = 0;
    BOOL known;

    if (argc < 2) {
        return 2;
    }
    event = (HANDLE)(uintptr_t)_strtoui64(argv[1], NULL, 10);
    known = GetHandleInformation(event, &flags);
    printf("child: inherited handle %p, usable = %d, flags = 0x%lx\n", (void *)event, known, flags);
    fflush(stdout);

    Sleep(1500);
    CloseHandle(event);
    printf("child: handle closed, exiting\n");
    return 0;
}