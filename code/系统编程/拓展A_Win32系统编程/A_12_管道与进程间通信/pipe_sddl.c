/* 用句柄取命名管道的默认安全描述符（SDDL）。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>

#define PIPE_NAME L"\\\\.\\pipe\\mdr_sddl"

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    HANDLE pipe = CreateNamedPipeW(PIPE_NAME, PIPE_ACCESS_DUPLEX,
                                   PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1,
                                   4096, 4096, 0, NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        printf("CreateNamedPipeW: error %lu\n", GetLastError());
        return 1;
    }

    PSECURITY_DESCRIPTOR sd = NULL;
    DWORD err = GetSecurityInfo(pipe, SE_KERNEL_OBJECT,
                                OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                                    DACL_SECURITY_INFORMATION,
                                NULL, NULL, NULL, NULL, &sd);
    if (err != ERROR_SUCCESS) {
        printf("GetSecurityInfo: error %lu\n", err);
    } else {
        LPWSTR text = NULL;
        if (ConvertSecurityDescriptorToStringSecurityDescriptorW(
                sd, SDDL_REVISION_1,
                OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                    DACL_SECURITY_INFORMATION,
                &text, NULL)) {
            wprintf(L"%ls\n", text);
            LocalFree(text);
        } else {
            printf("ConvertSecurityDescriptorToStringSecurityDescriptorW: error %lu\n",
                   GetLastError());
        }
        LocalFree(sd);
    }

    CloseHandle(pipe);
    return 0;
}