#include <windows.h>
#include <stdio.h>

int main(void) {
    HANDLE ev;
    HANDLE man;
    DWORD r;

    /* 自动重置事件：连 SetEvent 两次也只算一次 */
    ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    SetEvent(ev);
    SetEvent(ev);
    r = WaitForSingleObject(ev, 0);
    printf("auto-reset, wait #1: %s\n",
           r == WAIT_OBJECT_0 ? "signaled" : "timeout");
    r = WaitForSingleObject(ev, 0);
    printf("auto-reset, wait #2: %s\n",
           r == WAIT_OBJECT_0 ? "signaled" : "timeout");

    /* 手动重置事件：一直保持有信号，直到 ResetEvent */
    man = CreateEventW(NULL, TRUE, FALSE, NULL);
    SetEvent(man);
    r = WaitForSingleObject(man, 0);
    printf("manual-reset, wait #1: %s\n",
           r == WAIT_OBJECT_0 ? "signaled" : "timeout");
    r = WaitForSingleObject(man, 0);
    printf("manual-reset, wait #2: %s\n",
           r == WAIT_OBJECT_0 ? "signaled" : "timeout");
    ResetEvent(man);
    r = WaitForSingleObject(man, 0);
    printf("manual-reset after ResetEvent: %s\n",
           r == WAIT_OBJECT_0 ? "signaled" : "timeout");

    CloseHandle(ev);
    CloseHandle(man);
    return 0;
}