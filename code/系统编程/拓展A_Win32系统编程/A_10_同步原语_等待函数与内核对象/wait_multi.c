#include <windows.h>
#include <stdio.h>

#define N 3

static HANDLE g_events[N];

static void show(const char *what, DWORD r) {
    if (r < WAIT_OBJECT_0 + N) {
        printf("%-30s -> WAIT_OBJECT_0 + %lu\n", what,
               (unsigned long)(r - WAIT_OBJECT_0));
    } else if (r == WAIT_TIMEOUT) {
        printf("%-30s -> WAIT_TIMEOUT\n", what);
    } else {
        printf("%-30s -> %lu\n", what, (unsigned long)r);
    }
}

int main(void) {
    int i;

    for (i = 0; i < N; i++) {
        g_events[i] = CreateEventW(NULL, FALSE, FALSE, NULL);
    }

    SetEvent(g_events[2]);
    show("only [2] set", WaitForMultipleObjects(N, g_events, FALSE, 1000));

    SetEvent(g_events[0]);
    SetEvent(g_events[2]);
    show("[0] and [2] set", WaitForMultipleObjects(N, g_events, FALSE, 1000));
    show("again, nothing new", WaitForMultipleObjects(N, g_events, FALSE, 1000));

    show("nothing set", WaitForMultipleObjects(N, g_events, FALSE, 200));

    SetEvent(g_events[1]);
    SetEvent(g_events[2]);
    show("wait-all, only [1] and [2]",
         WaitForMultipleObjects(N, g_events, TRUE, 200));

    SetEvent(g_events[0]);
    show("wait-all, all three", WaitForMultipleObjects(N, g_events, TRUE, 200));
    show("states after wait-all",
         WaitForMultipleObjects(N, g_events, FALSE, 0));

    for (i = 0; i < N; i++) {
        CloseHandle(g_events[i]);
    }
    return 0;
}