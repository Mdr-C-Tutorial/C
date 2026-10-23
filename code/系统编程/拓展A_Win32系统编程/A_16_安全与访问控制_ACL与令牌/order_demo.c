/* 访问检查只看 ACE 的顺序：同样的两条 ACE 换个顺序，结果不同。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>

static int make_file(const wchar_t *path, const wchar_t *sddl) {
    PSECURITY_DESCRIPTOR sd = NULL;
    SECURITY_ATTRIBUTES sa;
    HANDLE f;

    /* 上一次运行留下的同名文件可能带着不允许覆盖的 DACL，先删掉。 */
    DeleteFileW(path);

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

static void show_dacl(const wchar_t *path) {
    PSECURITY_DESCRIPTOR sd = NULL;
    wchar_t *text = NULL;

    if (GetNamedSecurityInfoW((LPWSTR)path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL,
                              NULL, NULL, NULL, &sd) != ERROR_SUCCESS) {
        return;
    }
    if (ConvertSecurityDescriptorToStringSecurityDescriptorW(
            sd, SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &text, NULL)) {
        printf("  DACL: %ls\n", text);
        LocalFree(text);
    }
    LocalFree(sd);
}

static void try_read(const wchar_t *path) {
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (f == INVALID_HANDLE_VALUE) {
        printf("  只读打开失败: error %lu\n", GetLastError());
    } else {
        printf("  只读打开成功\n");
        CloseHandle(f);
    }
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    printf("先允许 BUILTIN\\Users 读，再拒绝:\n");
    make_file(L"order_allow_first.txt", L"D:P(A;;FR;;;BU)(D;;FR;;;BU)");
    show_dacl(L"order_allow_first.txt");
    try_read(L"order_allow_first.txt");

    printf("\n先拒绝 BUILTIN\\Users 读，再允许:\n");
    make_file(L"order_deny_first.txt", L"D:P(D;;FR;;;BU)(A;;FR;;;BU)");
    show_dacl(L"order_deny_first.txt");
    try_read(L"order_deny_first.txt");
    return 0;
}