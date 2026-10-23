/* inherit_parent.c：以 bInheritHandles 的两种取值启动子进程，观察句柄继承。 */
#include <stdio.h>
#include <wchar.h>
#include <windows.h>

#define PAD 64

static void run_child(HANDLE event, BOOL inherit, const wchar_t *label) {
    wchar_t cmd[512];
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;

    _snwprintf(cmd, 512, L"inherit_child.exe %llu", (unsigned long long)(uintptr_t)event);
    printf("%ls\n", label);
    fflush(stdout);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, inherit, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcessW failed: %lu\n", GetLastError());
        return;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

int main(void) {
    SECURITY_ATTRIBUTES sa = {.nLength = sizeof sa, .bInheritHandle = TRUE};
    HANDLE pad[PAD];
    HANDLE inheritable;
    HANDLE plain;

    /* 先占掉一批句柄槽，让后面两个句柄的值落在子进程自己的句柄表之外。 */
    for (int i = 0; i < PAD; i++) {
        pad[i] = CreateEventW(NULL, FALSE, FALSE, NULL);
    }
    inheritable = CreateEventW(&sa, TRUE, FALSE, NULL);
    plain = CreateEventW(NULL, TRUE, FALSE, NULL);

    printf("parent: inheritable event = %p, plain event = %p\n", (void *)inheritable,
           (void *)plain);
    fflush(stdout);
    SetEvent(inheritable);
    SetEvent(plain);

    run_child(inheritable, FALSE, L"bInheritHandles = FALSE:");
    run_child(inheritable, TRUE, L"bInheritHandles = TRUE:");
    run_child(plain, TRUE, L"bInheritHandles = TRUE, but created without bInheritHandle:");

    for (int i = 0; i < PAD; i++) {
        CloseHandle(pad[i]);
    }
    CloseHandle(inheritable);
    CloseHandle(plain);
    return 0;
}