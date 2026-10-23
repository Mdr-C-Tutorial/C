/* inherit_child.c：报告命令行给出的句柄在本进程里能否使用。 */
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

int main(int argc, char *argv[]) {
    BOOL set;
    DWORD error;

    if (argc < 2) {
        printf("usage: inherit_child <handle-value>\n");
        return 2;
    }

    HANDLE h = (HANDLE)(uintptr_t)_strtoui64(argv[1], NULL, 10);
    set = SetEvent(h);
    error = set ? 0 : GetLastError();
    printf("child: handle %p, SetEvent -> %d, error %lu\n", (void *)h, set, error);
    return set ? 0 : 1;
}