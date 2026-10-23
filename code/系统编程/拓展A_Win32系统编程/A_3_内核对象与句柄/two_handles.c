/* two_handles.c：两个句柄指向同一个事件；关掉一个，对象还在。 */
#include <stdio.h>
#include <windows.h>

#define EVENT_NAME L"Local\\MdrTutorial.TwoHandles"

int main(void) {
    HANDLE first = CreateEventW(NULL, TRUE, FALSE, EVENT_NAME);
    HANDLE second = OpenEventW(EVENT_ALL_ACCESS, FALSE, EVENT_NAME);
    HANDLE later;

    printf("first = %p, second = %p, same value = %d\n", (void *)first, (void *)second,
           first == second);

    CloseHandle(first);
    printf("first closed, SetEvent through second = %d\n", SetEvent(second));
    CloseHandle(second);

    later = OpenEventW(EVENT_ALL_ACCESS, FALSE, EVENT_NAME);
    printf("after both handles are closed, OpenEventW -> %p, error %lu\n", (void *)later,
           GetLastError());
    return 0;
}