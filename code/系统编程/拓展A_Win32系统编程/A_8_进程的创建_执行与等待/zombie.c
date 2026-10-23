/* zombie.c：子进程结束后进程对象还在，退出码可以反复查；
 * 最后一个句柄关闭后，这个 PID 就从系统里消失了。 */
#include <locale.h>
#include <stdio.h>
#include <wchar.h>
#include <windows.h>

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    wchar_t cmd[] = L"child.exe 5";
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;

    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcessW failed: %lu\n", GetLastError());
        return 1;
    }
    CloseHandle(pi.hThread);
    DWORD pid = pi.dwProcessId;

    WaitForSingleObject(pi.hProcess, INFINITE);
    Sleep(200);

    DWORD code = 0;

    GetExitCodeProcess(pi.hProcess, &code);
    printf("child %lu finished: exit code %lu\n", (unsigned long)pid, (unsigned long)code);
    printf("second WaitForSingleObject -> %lu\n",
           (unsigned long)WaitForSingleObject(pi.hProcess, 0));

    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

    printf("OpenProcess while the object lives -> %p\n", (void *)h);
    if (h != NULL) {
        CloseHandle(h);
    }
    CloseHandle(pi.hProcess);

    SetLastError(0);
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    printf("OpenProcess after the last handle closed -> %p, error %lu\n", (void *)h,
           GetLastError());
    return 0;
}