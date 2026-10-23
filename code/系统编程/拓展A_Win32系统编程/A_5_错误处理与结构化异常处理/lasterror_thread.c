#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

/* 每个线程有自己的最后错误码：线程里的失败不会改写主线程的 */
static DWORD WINAPI worker(void *param)
{
    HANDLE h = CreateFileW(L"worker_missing.txt", GENERIC_READ, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    (void)param;
    if (h == INVALID_HANDLE_VALUE) {
        printf("worker thread: GetLastError() = %lu\n",
               (unsigned long)GetLastError());
    }
    return 0;
}

int main(void)
{
    HANDLE h;
    HANDLE t;

    h = CreateFileW(L"C:\\Windows", GENERIC_READ, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("main thread before: GetLastError() = %lu\n",
               (unsigned long)GetLastError());
        t = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        WaitForSingleObject(t, INFINITE);
        CloseHandle(t);
        printf("main thread after:  GetLastError() = %lu\n",
               (unsigned long)GetLastError());
    }
    return 0;
}