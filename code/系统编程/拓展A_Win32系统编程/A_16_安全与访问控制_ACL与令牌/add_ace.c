/* 给已有文件追加 ACE：分别演示未保护与受保护的 DACL。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>

static void print_dacl(const wchar_t *path) {
    PSECURITY_DESCRIPTOR sd = NULL;
    wchar_t *sddl = NULL;

    if (GetNamedSecurityInfoW((LPWSTR)path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL,
                              NULL, NULL, NULL, &sd) != ERROR_SUCCESS) {
        printf("  GetNamedSecurityInfoW failed: error %lu\n", GetLastError());
        return;
    }
    if (ConvertSecurityDescriptorToStringSecurityDescriptorW(
            sd, SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &sddl, NULL)) {
        printf("  %ls\n", sddl);
        LocalFree(sddl);
    }
    LocalFree(sd);
}

/* 给文件追加一条允许 BUILTIN\Users 读取的 ACE。info 决定写回时的安全信息位。 */
static DWORD grant_users_read(const wchar_t *path, SECURITY_INFORMATION info) {
    BYTE sid_buf[SECURITY_MAX_SID_SIZE];
    DWORD sid_size = sizeof sid_buf;
    EXPLICIT_ACCESSW ea;
    PACL old_acl = NULL;
    PACL new_acl = NULL;
    PSECURITY_DESCRIPTOR sd = NULL;
    DWORD err;

    if (!CreateWellKnownSid(WinBuiltinUsersSid, NULL, sid_buf, &sid_size)) {
        return GetLastError();
    }

    ZeroMemory(&ea, sizeof ea);
    ea.grfAccessPermissions = GENERIC_READ;
    ea.grfAccessMode = GRANT_ACCESS;
    ea.grfInheritance = NO_INHERITANCE;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    ea.Trustee.ptstrName = (LPWSTR)sid_buf;

    err = GetNamedSecurityInfoW((LPWSTR)path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL,
                                NULL, &old_acl, NULL, &sd);
    if (err == ERROR_SUCCESS) {
        err = SetEntriesInAclW(1, &ea, old_acl, &new_acl);
    }
    if (err == ERROR_SUCCESS) {
        err = SetNamedSecurityInfoW((LPWSTR)path, SE_FILE_OBJECT, info, NULL, NULL, new_acl,
                                    NULL);
    }
    LocalFree(new_acl);
    LocalFree(sd);
    return err;
}

/* 以普通方式创建文件：不指定安全描述符，DACL 从当前目录继承。 */
static int create_plain(const wchar_t *path) {
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);

    if (f == INVALID_HANDLE_VALUE) {
        printf("创建 %ls 失败: error %lu\n", path, GetLastError());
        return 1;
    }
    CloseHandle(f);
    return 0;
}

/* 创建时显式指定受保护的 DACL：只有当前用户与 SYSTEM。 */
static int create_private(const wchar_t *path) {
    PSECURITY_DESCRIPTOR sd = NULL;
    SECURITY_ATTRIBUTES sa;
    HANDLE f;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;;FA;;;OW)(A;;FA;;;SY)", SDDL_REVISION_1, &sd, NULL)) {
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

int main(void) {
    DWORD err;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    printf("未保护的文件（继承自父目录）：\n");
    create_plain(L"plain.txt");
    print_dacl(L"plain.txt");
    err = grant_users_read(L"plain.txt", DACL_SECURITY_INFORMATION);
    printf("追加 Users 读取 -> %lu\n", (unsigned long)err);
    print_dacl(L"plain.txt");

    printf("\n受保护的文件（创建时指定 D:P）：\n");
    create_private(L"private.txt");
    print_dacl(L"private.txt");
    err = grant_users_read(L"private.txt", DACL_SECURITY_INFORMATION);
    printf("追加 Users 读取 -> %lu\n", (unsigned long)err);
    print_dacl(L"private.txt");

    printf("\n对受保护的文件显式解除保护：\n");
    err = grant_users_read(L"private.txt",
                           DACL_SECURITY_INFORMATION | UNPROTECTED_DACL_SECURITY_INFORMATION);
    printf("追加并解除保护 -> %lu\n", (unsigned long)err);
    print_dacl(L"private.txt");
    return 0;
}