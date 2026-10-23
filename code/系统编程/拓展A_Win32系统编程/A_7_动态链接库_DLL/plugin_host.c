#include "plugin.h"

#include <stdio.h>
#include <windows.h>

static void run_plugin(const wchar_t *path) {
    HMODULE mod = LoadLibraryW(path);
    if (mod == NULL) {
        printf("  LoadLibraryW failed: %lu\n", GetLastError());
        return;
    }

    plugin_name_fn name = (plugin_name_fn)(void (*)(void))GetProcAddress(mod, MDR_PLUGIN_ENTRY);
    plugin_run_fn run = (plugin_run_fn)(void (*)(void))GetProcAddress(mod, MDR_PLUGIN_RUN);
    if (name == NULL || run == NULL) {
        printf("  missing entry point, error %lu\n", GetLastError());
        FreeLibrary(mod);
        return;
    }

    printf("  %s(7) = %d\n", name(), run(7));
    FreeLibrary(mod);
}

int wmain(void) {
    wchar_t dir[MAX_PATH];
    if (GetModuleFileNameW(NULL, dir, MAX_PATH) == 0) {
        printf("GetModuleFileNameW failed: %lu\n", GetLastError());
        return 1;
    }
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (slash == NULL) {
        return 1;
    }
    *slash = L'\0';

    wchar_t pattern[MAX_PATH];
    _snwprintf(pattern, MAX_PATH, L"%ls\\plugins\\*.dll", dir);

    WIN32_FIND_DATAW fd;
    HANDLE find = FindFirstFileW(pattern, &fd);
    if (find == INVALID_HANDLE_VALUE) {
        printf("no plugins found (error %lu)\n", GetLastError());
        return 1;
    }

    int count = 0;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        wchar_t full[MAX_PATH];
        _snwprintf(full, MAX_PATH, L"%ls\\plugins\\%ls", dir, fd.cFileName);
        printf("loading %ls\n", full);
        run_plugin(full);
        count++;
    } while (FindNextFileW(find, &fd));
    FindClose(find);

    printf("%d plugin(s) loaded\n", count);
    return 0;
}