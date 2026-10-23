#include <stdio.h>
#include <windows.h>

static void where(const char *label, HMODULE m) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(m, path, MAX_PATH);
    if (n == 0) {
        printf("%s: cannot get path, error %lu\n", label, GetLastError());
        return;
    }
    printf("%s: %ls\n", label, path);
}

int main(void) {
    /* 程序目录里放了一个同名的假 version.dll，默认搜索顺序先看程序目录。 */
    HMODULE m = LoadLibraryW(L"version.dll");
    if (m != NULL) {
        where("default search        ", m);
        FreeLibrary(m);
    }

    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

    /* 加固后仍然先搜程序目录，所以假 DLL 还是会被加载。 */
    m = LoadLibraryW(L"version.dll");
    if (m != NULL) {
        where("after SetDefaultDllDir", m);
        FreeLibrary(m);
    }

    /* 明确限定只从 System32 找，拿到的才是系统那一份。 */
    m = LoadLibraryExW(L"version.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (m != NULL) {
        where("LOAD_LIBRARY_SEARCH_SYSTEM32", m);
        FreeLibrary(m);
    }
    return 0;
}