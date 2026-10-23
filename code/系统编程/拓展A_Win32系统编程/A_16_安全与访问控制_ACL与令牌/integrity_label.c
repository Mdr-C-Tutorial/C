/* 完整性标签：中等完整性进程能建中、低标签的对象，建不了高标签的。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>
#include <sddl.h>

static void make(const wchar_t *path, const wchar_t *sddl) {
    PSECURITY_DESCRIPTOR sd = NULL;
    SECURITY_ATTRIBUTES sa;
    HANDLE f;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sd,
                                                              NULL)) {
        printf("%-16ls SDDL 转换失败: error %lu\n", path, GetLastError());
        return;
    }
    sa.nLength = sizeof sa;
    sa.lpSecurityDescriptor = sd;
    sa.bInheritHandle = FALSE;
    f = CreateFileW(path, GENERIC_WRITE, 0, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    LocalFree(sd);
    if (f == INVALID_HANDLE_VALUE) {
        printf("%-16ls 创建失败: error %lu\n", path, GetLastError());
    } else {
        printf("%-16ls 创建成功\n", path);
        CloseHandle(f);
        DeleteFileW(path);
    }
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    make(L"label_medium.txt", L"D:P(A;;FA;;;WD)S:(ML;;NW;;;ME)");
    make(L"label_low.txt", L"D:P(A;;FA;;;WD)S:(ML;;NW;;;LW)");
    make(L"label_high.txt", L"D:P(A;;FA;;;WD)S:(ML;;NW;;;HI)");
    return 0;
}