/* NULL DACL 与空 DACL 的实际效果。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>
#include <sddl.h>

static int make_file(const wchar_t *path, const wchar_t *sddl) {
    PSECURITY_DESCRIPTOR sd = NULL;
    SECURITY_ATTRIBUTES sa;
    HANDLE f;

    /* 上一次运行留下的同名文件先删掉，空 DACL 的删不掉就算了。 */
    DeleteFileW(path);

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sd,
                                                              NULL)) {
        printf("转换 %ls 失败: error %lu\n", sddl, GetLastError());
        return 1;
    }
    sa.nLength = sizeof sa;
    sa.lpSecurityDescriptor = sd;
    sa.bInheritHandle = FALSE;
    f = CreateFileW(path, GENERIC_WRITE, 0, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    LocalFree(sd);
    if (f == INVALID_HANDLE_VALUE) {
        printf("CreateFileW %ls 失败: error %lu\n", path, GetLastError());
        return 1;
    }
    CloseHandle(f);
    printf("已创建 %ls\n", path);
    return 0;
}

static void try_open(const wchar_t *path) {
    HANDLE f = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);

    if (f == INVALID_HANDLE_VALUE) {
        printf("  以读写打开失败: error %lu\n", GetLastError());
    } else {
        printf("  以读写打开成功\n");
        CloseHandle(f);
    }
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    make_file(L"null_dacl.txt", L"D:NO_ACCESS_CONTROL");
    try_open(L"null_dacl.txt");
    make_file(L"empty_dacl.txt", L"D:");
    try_open(L"empty_dacl.txt");
    return 0;
}