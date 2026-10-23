#include <stdio.h>
#include <windows.h>

static DWORD WINAPI worker(LPVOID param) {
    (void)param;
    Sleep(50);
    return 0;
}

int wmain(int argc, wchar_t **argv) {
    const wchar_t *name = (argc > 1) ? argv[1] : L"trace_dll.dll";

    /* DllMain 里的 WriteFile 不经过 CRT 缓冲，宿主也关掉缓冲才能看到真实顺序。 */
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("host: before LoadLibraryW\n");
    HMODULE m = LoadLibraryW(name);
    if (m == NULL) {
        printf("host: LoadLibraryW failed, error %lu\n", GetLastError());
        return 1;
    }
    printf("host: after LoadLibraryW\n");

    HANDLE th = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    if (th != NULL) {
        WaitForSingleObject(th, INFINITE);
        CloseHandle(th);
    }
    printf("host: thread finished\n");

    printf("host: before FreeLibrary\n");
    FreeLibrary(m);
    printf("host: after FreeLibrary\n");
    return 0;
}