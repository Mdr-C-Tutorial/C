#include <stdio.h>
#include <windows.h>

typedef int (*add_fn)(int, int);
typedef char *(*greet_fn)(const char *);
typedef void (*free_fn)(void *);

int main(void) {
    HMODULE math = LoadLibraryW(L"mdrmath.dll");
    if (math == NULL) {
        printf("LoadLibraryW(mdrmath.dll) failed: %lu\n", GetLastError());
        return 1;
    }

    add_fn add = (add_fn)(void (*)(void))GetProcAddress(math, "mdr_add");
    greet_fn greet = (greet_fn)(void (*)(void))GetProcAddress(math, "mdr_greet");
    free_fn release = (free_fn)(void (*)(void))GetProcAddress(math, "mdr_buffer_free");
    if (add == NULL || greet == NULL || release == NULL) {
        printf("GetProcAddress failed: %lu\n", GetLastError());
        FreeLibrary(math);
        return 1;
    }
    printf("mdr_add(2, 3) = %d\n", add(2, 3));

    char *msg = greet("run-time linking");
    if (msg != NULL) {
        printf("%s\n", msg);
        release(msg);
    }

    /* 名字写错：与找不到 DLL 是两种错误，码不同。 */
    FARPROC missing = GetProcAddress(math, "mdr_sub");
    printf("GetProcAddress(mdr_sub) = %p, error %lu\n", (void *)missing, GetLastError());

    FreeLibrary(math);
    return 0;
}