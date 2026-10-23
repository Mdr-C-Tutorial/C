/* Administrators 在筛选后的令牌里只用于拒绝：DACL 只给 BA 的文件，自己反而读不了。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>
#include <sddl.h>

/* 按给定的 DACL 创建文件，返回 0 表示成功。 */
static int make(const wchar_t *path, const wchar_t *sddl) {
    PSECURITY_DESCRIPTOR sd = NULL;
    SECURITY_ATTRIBUTES sa;
    HANDLE f;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sd,
                                                              NULL)) {
        printf("SDDL 转换失败: error %lu\n", GetLastError());
        return 1;
    }
    sa.nLength = sizeof sa;
    sa.lpSecurityDescriptor = sd;
    sa.bInheritHandle = FALSE;
    f = CreateFileW(path, GENERIC_WRITE, 0, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    LocalFree(sd);
    if (f == INVALID_HANDLE_VALUE) {
        printf("创建 %ls 失败: error %lu\n", path, GetLastError());
        return 1;
    }
    CloseHandle(f);
    return 0;
}

static void try_open(const wchar_t *path, const wchar_t *what) {
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (f == INVALID_HANDLE_VALUE) {
        printf("%ls：失败，error %lu\n", what, GetLastError());
    } else {
        printf("%ls：成功\n", what);
        CloseHandle(f);
    }
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (make(L"admin_only.txt", L"D:P(A;;FA;;;BA)") != 0 ||
        make(L"users_only.txt", L"D:P(A;;FA;;;BU)") != 0) {
        return 1;
    }
    try_open(L"admin_only.txt", L"DACL 只给 BUILTIN\\Administrators");
    try_open(L"users_only.txt", L"DACL 只给 BUILTIN\\Users");
    return 0;
}