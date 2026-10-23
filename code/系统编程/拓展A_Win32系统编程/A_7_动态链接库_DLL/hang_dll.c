#include <windows.h>

/*
 * 故意违反 DllMain 的限制：在 DLL_PROCESS_ATTACH 里创建线程并等它结束。
 * 新线程在初始化时要拿加载器锁发 DLL_THREAD_ATTACH 通知，锁被当前 DllMain 持有，
 * 新线程等锁、DllMain 等线程，双方都不动。
 */
static DWORD WINAPI worker(LPVOID param) {
    (void)param;
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID reserved) {
    (void)self;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        HANDLE th = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (th != NULL) {
            WaitForSingleObject(th, INFINITE);
            CloseHandle(th);
        }
    }
    return TRUE;
}

__declspec(dllexport) int hang_ping(void) {
    return 1;
}