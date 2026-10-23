#include <stdio.h>
#include <windows.h>

typedef int (*bump_fn)(void);

int main(void) {
    HMODULE m = LoadLibraryW(L"counter_dll.dll");
    if (m == NULL) {
        printf("load failed: %lu\n", GetLastError());
        return 1;
    }
    bump_fn bump = (bump_fn)(void (*)(void))GetProcAddress(m, "counter_bump");
    if (bump == NULL) {
        printf("GetProcAddress failed: %lu\n", GetLastError());
        FreeLibrary(m);
        return 1;
    }

    int v1 = bump();
    int v2 = bump();
    int v3 = bump();
    printf("pid %lu: bumps %d %d %d\n", GetCurrentProcessId(), v1, v2, v3);
    FreeLibrary(m);
    return 0;
}