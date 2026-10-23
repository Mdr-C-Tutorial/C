/* 受限令牌：普通 SID 与限制 SID 两套检查都要通过才放行。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>
#include <sddl.h>

static void try_open(const wchar_t *path, const char *what) {
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (f == INVALID_HANDLE_VALUE) {
        printf("  %s: 失败，error %lu\n", what, GetLastError());
    } else {
        printf("  %s: 成功\n", what);
        CloseHandle(f);
    }
}

int main(void) {
    HANDLE token = NULL;
    HANDLE restricted = NULL;
    PSID null_sid = NULL;
    SID_AND_ATTRIBUTES restrict_sid;
    HANDLE normal;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    normal = CreateFileW(L"restrict_demo.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if (normal == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: error %lu\n", GetLastError());
        return 1;
    }
    CloseHandle(normal);

    printf("未受限：\n");
    try_open(L"restrict_demo.txt", "以当前用户读");

    /* S-1-0-0 是 Null SID，没有任何成员。把它当限制 SID，等于要求两套检查里
       都必须有它，而它永远不会匹配，于是所有访问都被拒绝。 */
    if (!ConvertStringSidToSidW(L"S-1-0-0", &null_sid)) {
        printf("ConvertStringSidToSidW failed: error %lu\n", GetLastError());
        return 1;
    }
    restrict_sid.Sid = null_sid;
    restrict_sid.Attributes = 0;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &token)) {
        printf("OpenProcessToken failed: error %lu\n", GetLastError());
        LocalFree(null_sid);
        return 1;
    }
    if (!CreateRestrictedToken(token, DISABLE_MAX_PRIVILEGE, 0, NULL, 0, NULL, 1,
                               &restrict_sid, &restricted)) {
        printf("CreateRestrictedToken failed: error %lu\n", GetLastError());
        CloseHandle(token);
        LocalFree(null_sid);
        return 1;
    }
    if (!ImpersonateLoggedOnUser(restricted)) {
        printf("ImpersonateLoggedOnUser failed: error %lu\n", GetLastError());
        CloseHandle(restricted);
        CloseHandle(token);
        LocalFree(null_sid);
        return 1;
    }

    printf("受限令牌（限制 SID 为 S-1-0-0，没有成员）：\n");
    try_open(L"restrict_demo.txt", "以受限令牌读");

    RevertToSelf();
    CloseHandle(restricted);
    CloseHandle(token);
    LocalFree(null_sid);
    return 0;
}