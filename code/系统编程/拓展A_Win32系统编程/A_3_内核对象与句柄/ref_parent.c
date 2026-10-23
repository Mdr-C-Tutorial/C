/* ref_parent.c：关闭自己的句柄后，只要子进程还持有一份，命名对象就仍然存在。 */
#include <stdio.h>
#include <windows.h>

#define EVENT_NAME L"Local\\MdrTutorial.Ref"

int main(void) {
    SECURITY_ATTRIBUTES sa = {.nLength = sizeof sa, .bInheritHandle = TRUE};
    HANDLE event = CreateEventW(&sa, TRUE, FALSE, EVENT_NAME);
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;
    wchar_t cmd[256];
    HANDLE again;

    _snwprintf(cmd, 256, L"ref_child.exe %llu", (unsigned long long)(uintptr_t)event);
    fflush(stdout);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcessW failed: %lu\n", GetLastError());
        return 1;
    }
    CloseHandle(pi.hThread);

    CloseHandle(event);
    event = NULL;
    printf("parent: closed its own handle\n");
    fflush(stdout);

    again = OpenEventW(EVENT_ALL_ACCESS, FALSE, EVENT_NAME);
    printf("parent: OpenEventW by name while the child holds one -> %p, error %lu\n",
           (void *)again, GetLastError());
    if (again != NULL) {
        CloseHandle(again);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);

    again = OpenEventW(EVENT_ALL_ACCESS, FALSE, EVENT_NAME);
    printf("parent: OpenEventW by name after the child exited -> %p, error %lu\n", (void *)again,
           GetLastError());
    return 0;
}