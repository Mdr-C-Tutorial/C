/* dup_parent.c：把事件句柄复制进子进程，子进程用它置位。 */
#include <stdio.h>
#include <wchar.h>
#include <windows.h>

int main(void) {
    wchar_t cmd[512];
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;
    HANDLE event = CreateEventW(NULL, TRUE, FALSE, NULL);

    if (event == NULL) {
        printf("CreateEventW failed: %lu\n", GetLastError());
        return 1;
    }
    printf("parent pid %lu, event handle %p\n", GetCurrentProcessId(), (void *)event);
    fflush(stdout);

    _snwprintf(cmd, 512, L"dup_child.exe %lu %llu", GetCurrentProcessId(),
               (unsigned long long)(uintptr_t)event);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcessW failed: %lu\n", GetLastError());
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    DWORD state = WaitForSingleObject(event, 0);
    printf("parent: event %s\n",
           state == WAIT_OBJECT_0 ? "is signaled by the child" : "is still nonsignaled");
    CloseHandle(event);
    return state == WAIT_OBJECT_0 ? 0 : 1;
}