#include <stdio.h>
#include <windows.h>

/* 用法：search_demo.exe <mdrmath.dll 的完整路径>。
 * 在一个只有 mdrmath.dll、没有 exe 的同级目录里运行，观察当前目录的作用。 */
int wmain(int argc, wchar_t **argv) {
    if (argc < 2) {
        printf("usage: search_demo.exe <full path to mdrmath.dll>\n");
        return 1;
    }

    HMODULE m = LoadLibraryW(L"mdrmath.dll");
    printf("before hardening: LoadLibraryW(\"mdrmath.dll\") ");
    if (m != NULL) {
        printf("ok, mapped at %p\n", (void *)m);
        FreeLibrary(m);
    } else {
        printf("failed, error %lu\n", GetLastError());
    }

    if (!SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)) {
        printf("SetDefaultDllDirectories failed: %lu\n", GetLastError());
        return 1;
    }

    m = LoadLibraryW(L"mdrmath.dll");
    printf("after hardening:  LoadLibraryW(\"mdrmath.dll\") ");
    if (m != NULL) {
        printf("ok, mapped at %p\n", (void *)m);
        FreeLibrary(m);
    } else {
        printf("failed, error %lu\n", GetLastError());
    }

    m = LoadLibraryW(argv[1]);
    printf("full path:        LoadLibraryW(argv[1]) ");
    if (m != NULL) {
        printf("ok, mapped at %p\n", (void *)m);
        FreeLibrary(m);
    } else {
        printf("failed, error %lu\n", GetLastError());
    }
    return 0;
}