/* 访问检查只在打开对象时做一次：已有句柄的权力不随 DACL 改变。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>

#define PATH L"check_once.txt"

static void show_dacl(HANDLE h) {
    PSECURITY_DESCRIPTOR sd = NULL;
    wchar_t *text = NULL;

    DWORD err = GetSecurityInfo(h, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL,
                                NULL, NULL, &sd);

    if (err == ERROR_SUCCESS) {
        if (ConvertSecurityDescriptorToStringSecurityDescriptorW(
                sd, SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &text, NULL)) {
            printf("  DACL: %ls\n", text);
            LocalFree(text);
        }
        LocalFree(sd);
    } else {
        printf("  GetSecurityInfo failed: error %lu\n", err);
    }
}

/* 通过一个已有的句柄把 DACL 设回只给当前用户与 SYSTEM 完全控制。 */
static int reset_dacl(HANDLE control, const wchar_t *user_sid) {
    wchar_t sddl[256];
    PSECURITY_DESCRIPTOR sd = NULL;
    PACL dacl = NULL;
    BOOL present;
    BOOL defaulted;
    DWORD err;

    _snwprintf(sddl, 256, L"D:P(A;;FA;;;%ls)(A;;FA;;;SY)", user_sid);
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sd,
                                                              NULL)) {
        return 1;
    }
    GetSecurityDescriptorDacl(sd, &present, &dacl, &defaulted);
    err = SetSecurityInfo(control, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL,
                          dacl, NULL);
    LocalFree(sd);
    return err == ERROR_SUCCESS ? 0 : 1;
}

/* 追加一条拒绝当前用户读取的 ACE。SetEntriesInAcl 把拒绝 ACE 放在最前面。 */
static int add_deny_read(HANDLE control, PSID me) {
    EXPLICIT_ACCESSW ea;
    PACL old_acl = NULL;
    PACL new_acl = NULL;
    PSECURITY_DESCRIPTOR sd = NULL;
    DWORD err;

    err = GetSecurityInfo(control, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL,
                          &old_acl, NULL, &sd);
    if (err != ERROR_SUCCESS) {
        printf("  GetSecurityInfo failed: error %lu\n", err);
        return 1;
    }
    ZeroMemory(&ea, sizeof ea);
    ea.grfAccessPermissions = GENERIC_READ;
    ea.grfAccessMode = DENY_ACCESS;
    ea.grfInheritance = NO_INHERITANCE;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
    ea.Trustee.ptstrName = (LPWSTR)me;

    err = SetEntriesInAclW(1, &ea, old_acl, &new_acl);
    if (err == ERROR_SUCCESS) {
        err = SetSecurityInfo(control, SE_FILE_OBJECT,
                              DACL_SECURITY_INFORMATION |
                                  PROTECTED_DACL_SECURITY_INFORMATION,
                              NULL, NULL, new_acl, NULL);
    }
    LocalFree(new_acl);
    LocalFree(sd);
    return err == ERROR_SUCCESS ? 0 : 1;
}

int main(void) {
    HANDLE token;
    TOKEN_USER *user;
    DWORD size;
    PSECURITY_DESCRIPTOR sd = NULL;
    SECURITY_ATTRIBUTES sa;
    PSID me;
    wchar_t *user_sid = NULL;
    wchar_t sddl[256];
    HANDLE writer;
    HANDLE reader;
    HANDLE control;
    char buf[16];
    DWORD got;
    HANDLE again;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    GetTokenInformation(token, TokenUser, NULL, 0, &size);
    user = malloc(size);
    GetTokenInformation(token, TokenUser, user, size, &size);
    me = user->User.Sid;
    ConvertSidToStringSidW(me, &user_sid);
    CloseHandle(token);

    _snwprintf(sddl, 256, L"D:P(A;;FA;;;%ls)(A;;FA;;;SY)", user_sid);
    ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sd, NULL);
    sa.nLength = sizeof sa;
    sa.lpSecurityDescriptor = sd;
    sa.bInheritHandle = FALSE;

    writer = CreateFileW(PATH, GENERIC_WRITE, 0, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                         NULL);
    LocalFree(sd);
    if (writer == INVALID_HANDLE_VALUE) {
        printf("创建文件失败: error %lu\n", GetLastError());
        return 1;
    }
    WriteFile(writer, "hello", 5, &got, NULL);
    CloseHandle(writer);

    reader = CreateFileW(PATH, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    control = CreateFileW(PATH, WRITE_DAC | READ_CONTROL,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL, NULL);
    if (reader == INVALID_HANDLE_VALUE || control == INVALID_HANDLE_VALUE) {
        printf("打开文件失败: error %lu\n", GetLastError());
        return 1;
    }
    printf("已用一个只读句柄和一个 WRITE_DAC 句柄打开文件\n");
    show_dacl(control);

    printf("\n通过已有的 WRITE_DAC 句柄加一条拒绝自己读取的 ACE:\n");
    if (add_deny_read(control, me) != 0) {
        printf("  追加拒绝 ACE 失败\n");
    }
    show_dacl(control);

    printf("\n已有的读句柄继续读:\n");
    if (ReadFile(reader, buf, sizeof buf - 1, &got, NULL) && got > 0) {
        buf[got] = '\0';
        printf("  ReadFile 成功，读到 \"%s\"\n", buf);
    } else {
        printf("  ReadFile 失败: error %lu\n", GetLastError());
    }

    printf("\n再新开一个只读句柄:\n");
    again = CreateFileW(PATH, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (again == INVALID_HANDLE_VALUE) {
        printf("  CreateFileW 失败: error %lu\n", GetLastError());
    } else {
        printf("  CreateFileW 成功\n");
        CloseHandle(again);
    }

    printf("\n通过仍然有效的 WRITE_DAC 句柄把 DACL 恢复原样:\n");
    reset_dacl(control, user_sid);
    again = CreateFileW(PATH, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (again == INVALID_HANDLE_VALUE) {
        printf("  新的只读句柄仍然失败: error %lu\n", GetLastError());
    } else {
        printf("  新的只读句柄打开成功\n");
        CloseHandle(again);
    }

    CloseHandle(reader);
    CloseHandle(control);
    LocalFree(user_sid);
    free(user);
    DeleteFileW(PATH);
    return 0;
}