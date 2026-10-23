/* handle_leak.c：启动 200 个子进程，比较关闭与不关闭进程/线程句柄时
 * 本进程的句柄数。子进程不带参数，所以不打印任何东西。 */
#include <stdio.h>
#include <windows.h>

#define N 200

static DWORD handle_count(void) {
    DWORD n = 0;

    GetProcessHandleCount(GetCurrentProcess(), &n);
    return n;
}

static void spawn(int close_handles) {
    for (int i = 0; i < N; i++) {
        wchar_t cmd[] = L"child.exe";
        STARTUPINFOW si = {.cb = sizeof si};
        PROCESS_INFORMATION pi;

        if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            printf("CreateProcessW failed: %lu\n", GetLastError());
            return;
        }
        WaitForSingleObject(pi.hProcess, INFINITE);
        if (close_handles) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
}

int main(void) {
    DWORD before = handle_count();

    spawn(1);
    DWORD after_close = handle_count();
    printf("closed:     %lu -> %lu\n", (unsigned long)before, (unsigned long)after_close);

    spawn(0);
    DWORD after_leak = handle_count();
    printf("not closed: %lu -> %lu\n", (unsigned long)after_close,
           (unsigned long)after_leak);
    return 0;
}