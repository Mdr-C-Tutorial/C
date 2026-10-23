#include <stdio.h>
#include <windows.h>

/* 用私有堆分配，释放必须走本 DLL 提供的函数，与调用方的 CRT 无关。 */
static HANDLE arena;

__declspec(dllexport) char *make_message(const char *name) {
    if (arena == NULL) {
        arena = HeapCreate(0, 0, 0);
        if (arena == NULL) {
            return NULL;
        }
    }
    size_t need = 0;
    while (name[need] != '\0') {
        need++;
    }
    need += 8;
    char *p = HeapAlloc(arena, 0, need);
    if (p == NULL) {
        return NULL;
    }
    snprintf(p, need, "hello, %s", name);
    return p;
}

__declspec(dllexport) void release_message(char *p) {
    if (arena != NULL && p != NULL) {
        HeapFree(arena, 0, p);
    }
}