/* 启用和禁用令牌中的特权，并检查 AdjustTokenPrivileges 的返回值与最后错误码。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

/* 查询一个特权当前的状态。返回 1 表示令牌中有这个特权，0 表示没有。 */
static int privilege_state(HANDLE token, const wchar_t *name, const char **state) {
    LUID luid;
    DWORD size = 0;
    TOKEN_PRIVILEGES *all;
    int found = 0;

    *state = "不在令牌中";
    if (!LookupPrivilegeValueW(NULL, name, &luid)) {
        printf("LookupPrivilegeValueW %ls failed: error %lu\n", name, GetLastError());
        return 0;
    }
    if (!GetTokenInformation(token, TokenPrivileges, NULL, 0, &size) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return 0;
    }
    all = malloc(size);
    if (all == NULL || !GetTokenInformation(token, TokenPrivileges, all, size, &size)) {
        free(all);
        return 0;
    }
    for (DWORD i = 0; i < all->PrivilegeCount; i++) {
        if (all->Privileges[i].Luid.LowPart == luid.LowPart &&
            all->Privileges[i].Luid.HighPart == luid.HighPart) {
            *state = (all->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED) ? "已启用"
                                                                            : "已禁用";
            found = 1;
            break;
        }
    }
    free(all);
    return found;
}

/* 启用或禁用一个特权。返回 0 表示成功，1 表示失败，2 表示令牌里没有这个特权。 */
static int set_privilege(HANDLE token, const wchar_t *name, BOOL enable, DWORD *error) {
    LUID luid;
    TOKEN_PRIVILEGES tp;

    *error = 0;
    if (!LookupPrivilegeValueW(NULL, name, &luid)) {
        *error = GetLastError();
        return 1;
    }
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;

    if (!AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL)) {
        *error = GetLastError();
        return 1;
    }
    /* 函数返回 TRUE 也可能什么都没做：必须看最后错误码。 */
    *error = GetLastError();
    if (*error == ERROR_NOT_ALL_ASSIGNED) {
        return 2;
    }
    return 0;
}

static void report(HANDLE token, const wchar_t *name, BOOL enable) {
    DWORD error;
    int rc = set_privilege(token, name, enable, &error);

    if (rc == 0) {
        printf("%ls: %s成功\n", name, enable ? "启用" : "禁用");
    } else if (rc == 2) {
        printf("%ls: 令牌中没有这个特权，错误码 %lu (ERROR_NOT_ALL_ASSIGNED)\n", name,
               (unsigned long)error);
    } else {
        printf("%ls: 调用失败，错误码 %lu\n", name, (unsigned long)error);
    }
}

int main(void) {
    HANDLE token;
    const char *state;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES,
                          &token)) {
        printf("OpenProcessToken failed: error %lu\n", GetLastError());
        return 1;
    }

    privilege_state(token, L"SeShutdownPrivilege", &state);
    printf("初始 SeShutdownPrivilege: %s\n", state);
    report(token, L"SeShutdownPrivilege", TRUE);
    privilege_state(token, L"SeShutdownPrivilege", &state);
    printf("启用后 SeShutdownPrivilege: %s\n", state);
    report(token, L"SeShutdownPrivilege", FALSE);
    privilege_state(token, L"SeShutdownPrivilege", &state);
    printf("禁用后 SeShutdownPrivilege: %s\n\n", state);

    report(token, L"SeDebugPrivilege", TRUE);

    CloseHandle(token);
    return 0;
}