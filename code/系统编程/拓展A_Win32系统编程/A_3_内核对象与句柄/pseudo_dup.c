/* pseudo_dup.c：伪句柄的值，以及用 DuplicateHandle 取得真实句柄。 */
#include <stdio.h>
#include <windows.h>

static DWORD handle_count(void) {
    DWORD n = 0;

    GetProcessHandleCount(GetCurrentProcess(), &n);
    return n;
}

int main(void) {
    HANDLE pseudo_process = GetCurrentProcess();
    HANDLE pseudo_thread = GetCurrentThread();
    HANDLE real = NULL;
    DWORD before = handle_count();

    printf("GetCurrentProcess() = %p\n", (void *)pseudo_process);
    printf("GetCurrentThread()  = %p\n", (void *)pseudo_thread);
    printf("CloseHandle(pseudo process) = %d\n", CloseHandle(pseudo_process));
    printf("CloseHandle(pseudo thread)  = %d\n", CloseHandle(pseudo_thread));
    printf("handle count before %lu, after closing the pseudo handles %lu\n", before,
           handle_count());

    if (!DuplicateHandle(GetCurrentProcess(), pseudo_process, GetCurrentProcess(), &real, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
        printf("DuplicateHandle failed: %lu\n", GetLastError());
        return 1;
    }
    printf("after DuplicateHandle: %p, count %lu\n", (void *)real, handle_count());
    printf("GetProcessId(real) = %lu, GetCurrentProcessId() = %lu\n", GetProcessId(real),
           GetCurrentProcessId());
    CloseHandle(real);
    return 0;
}