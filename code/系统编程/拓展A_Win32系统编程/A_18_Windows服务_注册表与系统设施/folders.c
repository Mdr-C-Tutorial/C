/* 已知文件夹（Known Folder）与环境变量展开。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -o folders.exe folders.c -lole32 -luuid
 */
#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <locale.h>
#include <stdio.h>
#include <wchar.h>

static void show_known_folder(const KNOWNFOLDERID *id, const wchar_t *name) {
    PWSTR path = NULL;
    HRESULT hr = SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, NULL, &path);

    if (FAILED(hr)) {
        wprintf(L"%-18ls: SHGetKnownFolderPath failed 0x%08lX\n", name, (unsigned long)hr);
        return;
    }
    wprintf(L"%-18ls: %ls\n", name, path);
    CoTaskMemFree(path);
}

static void show_expanded(const wchar_t *src) {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = ExpandEnvironmentStringsW(src, buf, (DWORD)(sizeof buf / sizeof buf[0]));

    if (n == 0 || n > sizeof buf / sizeof buf[0]) {
        wprintf(L"%-18ls: ExpandEnvironmentStringsW failed\n", src);
        return;
    }
    wprintf(L"%-18ls: %ls\n", src, buf);
}

/* 程序数据放在 LocalAppData 下，而不是程序自己所在的目录。 */
static void write_local_config(void) {
    PWSTR base = NULL;
    wchar_t dir[MAX_PATH];
    wchar_t file[MAX_PATH];
    HANDLE h;

    if (FAILED(SHGetKnownFolderPath(&FOLDERID_LocalAppData, KF_FLAG_DEFAULT, NULL, &base))) {
        return;
    }
    wchar_t parent[MAX_PATH];
    _snwprintf(parent, MAX_PATH, L"%ls\\MdrTutorial", base);
    _snwprintf(dir, MAX_PATH, L"%ls\\MdrTutorial\\Echo", base);
    CoTaskMemFree(base);

    CreateDirectoryW(parent, NULL);
    CreateDirectoryW(dir, NULL);
    _snwprintf(file, MAX_PATH, L"%ls\\echo.ini", dir);
    h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        const char text[] = "port=23456\r\ngreeting=hello\r\n";
        DWORD written = 0;
        WriteFile(h, text, (DWORD)(sizeof text - 1), &written, NULL);
        CloseHandle(h);
        wprintf(L"\nwrote %ls\n", file);
    }

    /* 演示完删掉，不在用户机器上留文件。 */
    DeleteFileW(file);
    RemoveDirectoryW(dir);
    RemoveDirectoryW(parent);
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))) {
        wprintf(L"CoInitializeEx failed\n");
        return 1;
    }

    show_known_folder(&FOLDERID_LocalAppData, L"LocalAppData");
    show_known_folder(&FOLDERID_RoamingAppData, L"RoamingAppData");
    show_known_folder(&FOLDERID_ProgramData, L"ProgramData");
    show_known_folder(&FOLDERID_Documents, L"Documents");
    show_known_folder(&FOLDERID_Profile, L"Profile");

    wprintf(L"\nenvironment variables:\n");
    show_expanded(L"%LOCALAPPDATA%");
    show_expanded(L"%APPDATA%");
    show_expanded(L"%USERPROFILE%");
    show_expanded(L"%TEMP%");
    show_expanded(L"%OS%");

    write_local_config();

    CoUninitialize();
    return 0;
}