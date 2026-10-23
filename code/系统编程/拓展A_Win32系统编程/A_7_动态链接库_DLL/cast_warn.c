#include <stdio.h>
#include <windows.h>

typedef int (*add_fn)(int, int);

int main(void) {
    HMODULE m = LoadLibraryW(L"mdrmath.dll");
    if (m == NULL) {
        printf("LoadLibraryW failed: %lu\n", GetLastError());
        return 1;
    }

    /* 直接转换 FARPROC，触发 -Wcast-function-type。 */
    add_fn bad = (add_fn)GetProcAddress(m, "mdr_add");
    printf("direct cast: %d\n", bad(2, 3));

    FreeLibrary(m);
    return 0;
}