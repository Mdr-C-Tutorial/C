/* dup_child.c：用 DuplicateHandle 从父进程的句柄表里复制一个事件句柄。 */
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage: dup_child <parent-pid> <handle-value>\n");
        return 2;
    }

    DWORD parent_pid = (DWORD)strtoul(argv[1], NULL, 10);
    HANDLE source = (HANDLE)(uintptr_t)_strtoui64(argv[2], NULL, 10);
    HANDLE parent = OpenProcess(PROCESS_DUP_HANDLE, FALSE, parent_pid);
    HANDLE copy = NULL;

    if (parent == NULL) {
        printf("child: OpenProcess failed: %lu\n", GetLastError());
        return 1;
    }
    if (!DuplicateHandle(parent, source, GetCurrentProcess(), &copy, EVENT_MODIFY_STATE, FALSE,
                         0)) {
        printf("child: DuplicateHandle failed: %lu\n", GetLastError());
        CloseHandle(parent);
        return 1;
    }
    printf("child: got a local handle %p for the parent's %p, SetEvent -> %d\n", (void *)copy,
           (void *)source, SetEvent(copy));
    CloseHandle(copy);
    CloseHandle(parent);
    return 0;
}