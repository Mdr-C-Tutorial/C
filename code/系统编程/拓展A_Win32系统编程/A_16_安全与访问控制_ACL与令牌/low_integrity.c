/* 把当前进程的令牌降到低完整性，再尝试写中完整性的对象。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>
#include <sddl.h>

static int lower_to_low(void) {
    HANDLE token = NULL;
    PSID low = NULL;
    TOKEN_MANDATORY_LABEL tml;
    DWORD size;
    int status = 1;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_DEFAULT, &token)) {
        printf("OpenProcessToken failed: error %lu\n", GetLastError());
        return 1;
    }
    if (!ConvertStringSidToSidW(L"S-1-16-4096", &low)) {
        printf("ConvertStringSidToSidW failed: error %lu\n", GetLastError());
        CloseHandle(token);
        return 1;
    }
    size = GetLengthSid(low);
    tml.Label.Sid = low;
    tml.Label.Attributes = SE_GROUP_INTEGRITY;
    if (!SetTokenInformation(token, TokenIntegrityLevel, &tml,
                             sizeof(TOKEN_MANDATORY_LABEL) + size)) {
        printf("SetTokenInformation failed: error %lu\n", GetLastError());
    } else {
        printf("令牌已降到低完整性\n");
        status = 0;
    }
    LocalFree(low);
    CloseHandle(token);
    return status;
}

static void try_create(const wchar_t *path, const char *what) {
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);

    if (f == INVALID_HANDLE_VALUE) {
        printf("  %s：失败，error %lu\n", what, GetLastError());
    } else {
        printf("  %s：成功\n", what);
        CloseHandle(f);
    }
}

int main(void) {
    HANDLE f;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    f = CreateFileW(L"medium_target.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        CloseHandle(f);
    }
    CreateDirectoryW(L"medium_dir", NULL);

    printf("中完整性时：\n");
    try_create(L"medium_target.txt", "改写 medium_target.txt");
    try_create(L"medium_dir\\new.txt", "在 medium_dir 里新建文件");

    if (lower_to_low() != 0) {
        return 1;
    }
    printf("降到低完整性后：\n");
    try_create(L"medium_target.txt", "改写 medium_target.txt");
    try_create(L"medium_dir\\new.txt", "在 medium_dir 里新建文件");
    return 0;
}