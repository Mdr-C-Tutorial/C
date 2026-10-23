/* named_objects.c：同名对象的复用、类型冲突、名字区分大小写与生命周期。 */
#include <stdio.h>
#include <windows.h>

#define MUTEX_NAME L"Local\\MdrTutorial.NamedObject"

int main(void) {
    HANDLE first = CreateMutexW(NULL, FALSE, MUTEX_NAME);
    DWORD err_first = GetLastError();
    HANDLE second = CreateMutexW(NULL, FALSE, MUTEX_NAME);
    DWORD err_second = GetLastError();

    printf("first  CreateMutexW -> %p, last error %lu\n", (void *)first, err_first);
    printf("second CreateMutexW -> %p, last error %lu (ERROR_ALREADY_EXISTS = %ld)\n",
           (void *)second, err_second, ERROR_ALREADY_EXISTS);

    HANDLE clash = CreateEventW(NULL, FALSE, FALSE, MUTEX_NAME);
    printf("CreateEventW with the mutex's name -> %p, error %lu (ERROR_INVALID_HANDLE = %ld)\n",
           (void *)clash, GetLastError(), ERROR_INVALID_HANDLE);

    HANDLE lower = CreateEventW(NULL, FALSE, FALSE, L"Local\\MdrTutorial.Namedobject");
    printf("a name differing in case -> %p, error %lu\n", (void *)lower, GetLastError());

    CloseHandle(first);
    CloseHandle(second);
    CloseHandle(lower);

    first = CreateMutexW(NULL, FALSE, MUTEX_NAME);
    printf("after all handles are closed, CreateMutexW -> %p, last error %lu\n",
           (void *)first, GetLastError());
    CloseHandle(first);
    return 0;
}