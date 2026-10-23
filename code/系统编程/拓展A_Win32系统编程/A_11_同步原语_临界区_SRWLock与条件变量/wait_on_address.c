/* WaitOnAddress / WakeByAddressSingle：一个线程等状态变化，另一个线程修改并唤醒。 */
#include <stdio.h>
#include <windows.h>

static volatile LONG g_state = 0;

static DWORD WINAPI waiter(LPVOID arg) {
    (void)arg;
    LONG observed = InterlockedCompareExchange(&g_state, 0, 0);

    printf("waiter: state = %ld, going to sleep\n", observed);
    while (observed == 0) {
        /* 比较的是进入等待那一刻的值；值已经变了就立即返回 */
        WaitOnAddress((volatile VOID *)&g_state, &observed, sizeof observed, INFINITE);
        observed = g_state;
    }
    printf("waiter: woke up, state = %ld\n", observed);
    return 0;
}

static DWORD WINAPI setter(LPVOID arg) {
    (void)arg;
    Sleep(200);
    InterlockedExchange(&g_state, 1);
    printf("setter: state = 1, waking one\n");
    WakeByAddressSingle((PVOID)&g_state);
    return 0;
}

int main(void) {
    HANDLE threads[2];

    threads[0] = CreateThread(NULL, 0, waiter, NULL, 0, NULL);
    threads[1] = CreateThread(NULL, 0, setter, NULL, 0, NULL);
    if (threads[0] == NULL || threads[1] == NULL) {
        printf("CreateThread failed: %lu\n", GetLastError());
        return 1;
    }
    WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    CloseHandle(threads[0]);
    CloseHandle(threads[1]);
    return 0;
}