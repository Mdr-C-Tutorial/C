/* 为新文件指定只允许当前用户与 SYSTEM 访问的 DACL。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <sddl.h>

int main(int argc, char **argv) {
    HANDLE token = NULL;
    TOKEN_USER *user = NULL;
    DWORD size;
    wchar_t *user_text = NULL;
    wchar_t sddl[256];
    PSECURITY_DESCRIPTOR sd = NULL;
    SECURITY_ATTRIBUTES sa;
    wchar_t wpath[MAX_PATH];
    HANDLE file = INVALID_HANDLE_VALUE;
    DWORD written;
    int status = 1;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        printf("用法: private_file 文件\n");
        return 2;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, wpath, MAX_PATH) == 0) {
        printf("路径转换失败\n");
        return 1;
    }

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        printf("OpenProcessToken failed: error %lu\n", GetLastError());
        return 1;
    }
    if (!GetTokenInformation(token, TokenUser, NULL, 0, &size) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        printf("GetTokenInformation failed: error %lu\n", GetLastError());
        goto done;
    }
    user = malloc(size);
    if (user == NULL) {
        goto done;
    }
    if (!GetTokenInformation(token, TokenUser, user, size, &size)) {
        printf("GetTokenInformation failed: error %lu\n", GetLastError());
        goto done;
    }
    if (!ConvertSidToStringSidW(user->User.Sid, &user_text)) {
        printf("ConvertSidToStringSidW failed: error %lu\n", GetLastError());
        goto done;
    }

    /* D:P 表示 DACL 受保护，不从父目录继承；两条 ACE 分别给当前用户和 SYSTEM 完全控制。 */
    _snwprintf(sddl, 256, L"D:P(A;;FA;;;%ls)(A;;FA;;;SY)", user_text);
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            sddl, SDDL_REVISION_1, &sd, NULL)) {
        printf("ConvertStringSecurityDescriptorToSecurityDescriptorW failed: error %lu\n",
               GetLastError());
        goto done;
    }

    sa.nLength = sizeof sa;
    sa.lpSecurityDescriptor = sd;
    sa.bInheritHandle = FALSE;

    file = CreateFileW(wpath, GENERIC_WRITE, 0, &sa, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: error %lu\n", GetLastError());
        goto done;
    }
    if (!WriteFile(file, "private\n", 8, &written, NULL)) {
        printf("WriteFile failed: error %lu\n", GetLastError());
        goto done;
    }
    printf("已创建 %ls\n", wpath);
    status = 0;

done:
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
    if (sd != NULL) {
        LocalFree(sd);
    }
    LocalFree(user_text);
    free(user);
    if (token != NULL) {
        CloseHandle(token);
    }
    return status;
}