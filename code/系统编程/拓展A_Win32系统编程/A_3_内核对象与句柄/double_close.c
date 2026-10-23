/* double_close.c：重复关闭，以及句柄值被复用后关掉别人的对象。 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    HANDLE event = CreateEventW(NULL, TRUE, FALSE, NULL);
    BOOL first = CloseHandle(event);
    BOOL second = CloseHandle(event);
    DWORD err = GetLastError();

    printf("event %p: first CloseHandle = %d, second CloseHandle = %d, error %lu\n",
           (void *)event, first, second, err);

    HANDLE a = CreateEventW(NULL, TRUE, FALSE, NULL);
    CloseHandle(a);
    HANDLE b = CreateEventW(NULL, TRUE, FALSE, NULL);
    printf("a = %p, b = %p: the slot is reused = %d\n", (void *)a, (void *)b, a == b);

    BOOL close_a_again = CloseHandle(a);
    DWORD err_a = GetLastError();
    BOOL set_b = SetEvent(b);
    printf("CloseHandle(a) after reuse = %d, error %lu\n", close_a_again, err_a);
    printf("SetEvent(b) afterwards = %d, error %lu\n", set_b, GetLastError());

    BOOL close_null = CloseHandle(NULL);
    printf("CloseHandle(NULL) = %d, error %lu\n", close_null, GetLastError());
    return 0;
}