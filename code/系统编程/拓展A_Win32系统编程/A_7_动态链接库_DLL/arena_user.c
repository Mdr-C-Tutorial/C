#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

typedef char *(*make_fn)(const char *);
typedef void (*release_fn)(char *);

/* arena_user.exe            用 DLL 提供的函数释放
 * arena_user.exe bad        故意用调用方的 free 释放 */
int main(int argc, char **argv) {
    HMODULE m = LoadLibraryW(L"arena_dll.dll");
    if (m == NULL) {
        printf("LoadLibraryW failed: %lu\n", GetLastError());
        return 1;
    }
    make_fn make = (make_fn)(void (*)(void))GetProcAddress(m, "make_message");
    release_fn release = (release_fn)(void (*)(void))GetProcAddress(m, "release_message");
    if (make == NULL || release == NULL) {
        printf("GetProcAddress failed: %lu\n", GetLastError());
        FreeLibrary(m);
        return 1;
    }

    char *p = make("caller");
    if (p == NULL) {
        FreeLibrary(m);
        return 1;
    }
    printf("%s\n", p);
    fflush(stdout);

    if (argc > 1 && argv[1][0] == 'b') {
        /* 错误做法：调用方用自己的 CRT 释放 DLL 私有堆上的块。 */
        free(p);
        printf("survived free()\n");
    } else {
        release(p);
        printf("released with the DLL's own function\n");
    }
    FreeLibrary(m);
    return 0;
}