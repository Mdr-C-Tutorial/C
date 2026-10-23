/* 不指定安全描述符时，新文件从父目录继承；父目录不让继承时，用令牌里的默认 DACL。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>

static void show(const wchar_t *path) {
    PSECURITY_DESCRIPTOR sd = NULL;
    wchar_t *text = NULL;
    DWORD err;

    err = GetNamedSecurityInfoW((LPWSTR)path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL,
                                NULL, NULL, NULL, &sd);
    if (err != ERROR_SUCCESS) {
        printf("  %ls: GetNamedSecurityInfoW failed: error %lu\n", path, err);
        return;
    }
    if (ConvertSecurityDescriptorToStringSecurityDescriptorW(
            sd, SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &text, NULL)) {
        printf("  %ls\n    %ls\n", path, text);
        LocalFree(text);
    }
    LocalFree(sd);
}

static void show_default_dacl(void) {
    HANDLE token;
    DWORD size = 0;
    TOKEN_DEFAULT_DACL *dd;

    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    GetTokenInformation(token, TokenDefaultDacl, NULL, 0, &size);
    dd = malloc(size);
    if (dd != NULL && GetTokenInformation(token, TokenDefaultDacl, dd, size, &size)) {
        printf("令牌的默认 DACL 有 %lu 条 ACE\n",
               dd->DefaultDacl ? (unsigned long)dd->DefaultDacl->AceCount : 0);
    }
    free(dd);
    CloseHandle(token);
}

int main(void) {
    HANDLE f;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    show_default_dacl();

    f = CreateFileW(L"plain.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        CloseHandle(f);
    }
    printf("当前目录允许继承时：\n");
    show(L"plain.txt");

    CreateDirectoryW(L"isolated", NULL);
    {
        PSECURITY_DESCRIPTOR sd = NULL;
        PACL dacl = NULL;
        BOOL present;
        BOOL defaulted;

        /* 目录本身给我们完全控制，但 DACL 受保护，没有可继承给子对象的 ACE。 */
        ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;;FA;;;OW)(A;;FA;;;SY)", SDDL_REVISION_1, &sd, NULL);
        GetSecurityDescriptorDacl(sd, &present, &dacl, &defaulted);
        SetNamedSecurityInfoW(L"isolated", SE_FILE_OBJECT,
                              DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                              NULL, NULL, dacl, NULL);
        LocalFree(sd);
    }
    f = CreateFileW(L"isolated\\plain.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        CloseHandle(f);
    }
    printf("父目录受保护、没有可继承的 ACE 时：\n");
    show(L"isolated\\plain.txt");
    return 0;
}