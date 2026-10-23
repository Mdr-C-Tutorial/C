#include <stdio.h>
#include <windows.h>

int main(void) {
    printf("host: calling LoadLibraryW(hang_dll.dll)\n");
    fflush(stdout);
    HMODULE m = LoadLibraryW(L"hang_dll.dll");
    printf("host: LoadLibraryW returned %p, error %lu\n", (void *)m, m ? 0UL : GetLastError());
    if (m != NULL) {
        FreeLibrary(m);
    }
    return 0;
}