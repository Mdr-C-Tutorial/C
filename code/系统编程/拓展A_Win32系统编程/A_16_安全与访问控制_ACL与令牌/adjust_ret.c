/* AdjustTokenPrivileges 的返回值与最后错误码：返回值 TRUE 不代表特权真的启用了。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>

int main(void) {
    HANDLE token;
    LUID luid;
    TOKEN_PRIVILEGES tp;
    BOOL ok;
    DWORD err;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES,
                          &token)) {
        printf("OpenProcessToken failed: error %lu\n", GetLastError());
        return 1;
    }
    if (!LookupPrivilegeValueW(NULL, L"SeDebugPrivilege", &luid)) {
        printf("LookupPrivilegeValueW failed: error %lu\n", GetLastError());
        CloseHandle(token);
        return 1;
    }
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    /* 先把最后错误码设成一个无关的值，避免把上一次调用的残留当成结果。 */
    SetLastError(0xFFFFFFFF);
    ok = AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL);
    err = GetLastError();
    printf("AdjustTokenPrivileges -> %d, GetLastError -> %lu\n", ok, (unsigned long)err);

    CloseHandle(token);
    return 0;
}