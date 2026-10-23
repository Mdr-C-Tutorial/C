#include <windows.h>

/*
 * DllMain 里不依赖 CRT（CRT 可能还没初始化），用 WriteFile 直接写标准输出。
 * 每次通知写一行，形如 "[trace] DLL_PROCESS_ATTACH\n"。
 */
static void note(const char *text) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out == NULL || out == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD len = 0;
    while (text[len] != '\0') {
        len++;
    }
    DWORD written = 0;
    WriteFile(out, text, len, &written, NULL);
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID reserved) {
    (void)self;
    (void)reserved;
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        note("[trace] DLL_PROCESS_ATTACH\n");
        break;
    case DLL_THREAD_ATTACH:
        note("[trace] DLL_THREAD_ATTACH\n");
        break;
    case DLL_THREAD_DETACH:
        note("[trace] DLL_THREAD_DETACH\n");
        break;
    case DLL_PROCESS_DETACH:
        note("[trace] DLL_PROCESS_DETACH\n");
        break;
    default:
        break;
    }
    return TRUE;
}

__declspec(dllexport) int trace_ping(void) {
    return 42;
}