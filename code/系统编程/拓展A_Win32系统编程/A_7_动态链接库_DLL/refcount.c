#include <stdio.h>
#include <windows.h>

int main(void) {
    HMODULE a = LoadLibraryW(L"mdrmath.dll");
    HMODULE b = LoadLibraryW(L"mdrmath.dll");
    printf("two LoadLibraryW: %p %p\n", (void *)a, (void *)b);
    printf("GetModuleHandleW now: %p\n", (void *)GetModuleHandleW(L"mdrmath.dll"));

    FreeLibrary(b);
    printf("after one FreeLibrary: %p\n", (void *)GetModuleHandleW(L"mdrmath.dll"));

    FreeLibrary(a);
    printf("after two FreeLibrary: %p\n", (void *)GetModuleHandleW(L"mdrmath.dll"));
    return 0;
}