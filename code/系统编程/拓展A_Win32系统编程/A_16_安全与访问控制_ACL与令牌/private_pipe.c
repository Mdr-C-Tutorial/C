/* 命名管道的 DACL：先看默认安全描述符，再用只允许当前用户与 SYSTEM 的 SDDL 创建。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>

#define PIPE_NAME L"\\\\.\\pipe\\mdr_private_demo"

static void show_dacl(HANDLE h, const char *title) {
    PSECURITY_DESCRIPTOR sd = NULL;
    wchar_t *sddl = NULL;
    DWORD err;

    err = GetSecurityInfo(h, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, NULL,
                          NULL, NULL, NULL, &sd);
    if (err != ERROR_SUCCESS) {
        printf("%s: GetSecurityInfo failed: error %lu\n", title, err);
        return;
    }
    if (!ConvertSecurityDescriptorToStringSecurityDescriptorW(
            sd, SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &sddl, NULL)) {
        printf("%s: 转换 SDDL 失败: error %lu\n", title, GetLastError());
        LocalFree(sd);
        return;
    }
    printf("%s:\n  %ls\n", title, sddl);
    LocalFree(sddl);
    LocalFree(sd);
}

static PSECURITY_DESCRIPTOR private_sd(void) {
    HANDLE token = NULL;
    TOKEN_USER *user = NULL;
    DWORD size;
    wchar_t *user_text = NULL;
    wchar_t sddl[256];
    PSECURITY_DESCRIPTOR sd = NULL;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return NULL;
    }
    if (!GetTokenInformation(token, TokenUser, NULL, 0, &size) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        CloseHandle(token);
        return NULL;
    }
    user = malloc(size);
    if (user != NULL &&
        GetTokenInformation(token, TokenUser, user, size, &size) &&
        ConvertSidToStringSidW(user->User.Sid, &user_text)) {
        _snwprintf(sddl, 256, L"D:P(A;;FA;;;%ls)(A;;FA;;;SY)", user_text);
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
                sddl, SDDL_REVISION_1, &sd, NULL)) {
            sd = NULL;
        }
    }
    LocalFree(user_text);
    free(user);
    CloseHandle(token);
    return sd;
}

int main(void) {
    HANDLE def_pipe;
    HANDLE priv_pipe;
    PSECURITY_DESCRIPTOR sd;
    SECURITY_ATTRIBUTES sa;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    /* 不指定 SECURITY_ATTRIBUTES 时，管道使用创建者令牌里的默认 DACL。 */
    def_pipe = CreateNamedPipeW(L"\\\\.\\pipe\\mdr_default_demo",
                                PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                1, 4096, 4096, 0, NULL);
    if (def_pipe == INVALID_HANDLE_VALUE) {
        printf("CreateNamedPipeW(default) failed: error %lu\n", GetLastError());
        return 1;
    }
    show_dacl(def_pipe, "默认安全描述符的管道");
    CloseHandle(def_pipe);

    sd = private_sd();
    if (sd == NULL) {
        printf("构造安全描述符失败: error %lu\n", GetLastError());
        return 1;
    }
    sa.nLength = sizeof sa;
    sa.lpSecurityDescriptor = sd;
    sa.bInheritHandle = FALSE;

    priv_pipe = CreateNamedPipeW(PIPE_NAME,
                                 PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                 PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT |
                                     PIPE_REJECT_REMOTE_CLIENTS,
                                 1, 4096, 4096, 0, &sa);
    LocalFree(sd);
    if (priv_pipe == INVALID_HANDLE_VALUE) {
        printf("CreateNamedPipeW(private) failed: error %lu\n", GetLastError());
        return 1;
    }
    show_dacl(priv_pipe, "私有安全描述符的管道");

    /* 当前用户在这条 DACL 里，可以正常打开；别的标准用户会被拒绝。 */
    {
        HANDLE client = CreateFileW(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                    OPEN_EXISTING, 0, NULL);

        if (client == INVALID_HANDLE_VALUE) {
            printf("客户端连接失败: error %lu\n", GetLastError());
        } else {
            printf("客户端以当前用户身份连接成功\n");
            CloseHandle(client);
        }
    }

    CloseHandle(priv_pipe);
    return 0;
}