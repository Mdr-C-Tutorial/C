/* rights.c：同一个事件的几个句柄带着不同的访问权限。 */
#include <stdio.h>
#include <windows.h>

#define EVENT_NAME L"Local\\MdrTutorial.Rights"

int main(void) {
    HANDLE owner = CreateEventW(NULL, TRUE, FALSE, EVENT_NAME);
    HANDLE modify_only = OpenEventW(EVENT_MODIFY_STATE, FALSE, EVENT_NAME);
    HANDLE sync_only = OpenEventW(SYNCHRONIZE, FALSE, EVENT_NAME);
    HANDLE none = NULL;
    HANDLE same = NULL;
    DWORD wait;
    BOOL set;

    set = SetEvent(modify_only);
    wait = WaitForSingleObject(modify_only, 0);
    printf("EVENT_MODIFY_STATE handle %p: SetEvent -> %d, wait -> %lu, error %lu\n",
           (void *)modify_only, set, (unsigned long)wait, GetLastError());
    CloseHandle(modify_only);

    wait = WaitForSingleObject(sync_only, 0);
    SetLastError(0);
    set = SetEvent(sync_only);
    printf("SYNCHRONIZE handle        %p: wait -> %lu, SetEvent -> %d, error %lu\n",
           (void *)sync_only, (unsigned long)wait, set, GetLastError());
    CloseHandle(sync_only);

    DuplicateHandle(GetCurrentProcess(), owner, GetCurrentProcess(), &none, 0, FALSE, 0);
    SetLastError(0);
    set = SetEvent(none);
    printf("DuplicateHandle with access 0 -> %p: SetEvent -> %d, error %lu\n", (void *)none, set,
           GetLastError());
    CloseHandle(none);

    DuplicateHandle(GetCurrentProcess(), owner, GetCurrentProcess(), &same, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    set = SetEvent(same);
    printf("DuplicateHandle with DUPLICATE_SAME_ACCESS -> %p: SetEvent -> %d\n", (void *)same, set);
    CloseHandle(same);

    CloseHandle(owner);
    return 0;
}