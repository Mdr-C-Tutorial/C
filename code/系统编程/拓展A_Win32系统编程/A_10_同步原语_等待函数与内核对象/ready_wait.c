#include <windows.h>
#include <stdio.h>

#define EVENT_NAME L"Local\\MdrTutorial.Ready"

int main(void) {
    HANDLE ready;
    DWORD r;

    /* 手动重置事件：先由等待方创建，另一个进程按名字打开 */
    ready = CreateEventW(NULL, TRUE, FALSE, EVENT_NAME);
    if (ready == NULL) {
        printf("CreateEventW failed: %lu\n", GetLastError());
        return 1;
    }
    printf("waiting for %ls ...\n", EVENT_NAME);

    r = WaitForSingleObject(ready, 10000);
    if (r == WAIT_OBJECT_0) {
        printf("signaled, working\n");
    } else if (r == WAIT_TIMEOUT) {
        printf("timed out after 10 s\n");
    } else {
        printf("wait failed: %lu\n", GetLastError());
    }

    CloseHandle(ready);
    return r == WAIT_OBJECT_0 ? 0 : 1;
}