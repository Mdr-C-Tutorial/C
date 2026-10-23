#include <windows.h>
#include <stdio.h>

#define EVENT_NAME L"Local\\MdrTutorial.Ready"

int main(void) {
    HANDLE ready;

    /* EVENT_MODIFY_STATE 就够调用 SetEvent */
    ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, EVENT_NAME);
    if (ready == NULL) {
        printf("OpenEventW failed: %lu\n", GetLastError());
        return 1;
    }
    if (!SetEvent(ready)) {
        printf("SetEvent failed: %lu\n", GetLastError());
        CloseHandle(ready);
        return 1;
    }
    printf("event %ls set\n", EVENT_NAME);
    CloseHandle(ready);
    return 0;
}