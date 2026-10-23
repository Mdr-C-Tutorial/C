/* handles.c：三个事件句柄的值与进程的句柄数。 */
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

static DWORD handle_count(void) {
    DWORD n = 0;

    if (!GetProcessHandleCount(GetCurrentProcess(), &n)) {
        printf("GetProcessHandleCount failed: %lu\n", GetLastError());
    }
    return n;
}

int main(void) {
    HANDLE events[3];
    DWORD flags = 0;
    BOOL known;
    int i;

    printf("handle count before: %lu\n", handle_count());

    for (i = 0; i < 3; i++) {
        events[i] = CreateEventW(NULL, FALSE, FALSE, NULL);
        printf("event[%d] = %p, value %% 4 = %llu\n", i, (void *)events[i],
               (unsigned long long)((uintptr_t)events[i] % 4));
    }
    printf("handle count with three events open: %lu\n", handle_count());

    CloseHandle(events[0]);
    events[0] = CreateEventW(NULL, FALSE, FALSE, NULL);
    printf("after closing event[0], a new event gets %p\n", (void *)events[0]);

    for (i = 0; i < 3; i++) {
        CloseHandle(events[i]);
        events[i] = NULL;
    }
    printf("handle count after closing all: %lu\n", handle_count());

    /* 私有堆的返回值类型也叫 HANDLE，但它不在进程的句柄表里。 */
    HANDLE heap = HeapCreate(0, 0, 0);
    SetLastError(0);
    known = GetHandleInformation(heap, &flags);
    printf("heap %p: GetHandleInformation -> %d, error %lu, count %lu\n", (void *)heap, known,
           GetLastError(), handle_count());
    printf("CloseHandle(heap) -> %d, error %lu\n", CloseHandle(heap), GetLastError());
    HeapDestroy(heap);
    return 0;
}