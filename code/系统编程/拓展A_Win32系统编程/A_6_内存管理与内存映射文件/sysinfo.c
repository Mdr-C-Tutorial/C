#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

int main(void) {
    SYSTEM_INFO si;

    GetSystemInfo(&si);
    printf("page size              : %lu bytes\n", (unsigned long)si.dwPageSize);
    printf("allocation granularity : %lu bytes\n",
           (unsigned long)si.dwAllocationGranularity);
    printf("lowest user address    : %p\n", si.lpMinimumApplicationAddress);
    printf("highest user address   : %p\n", si.lpMaximumApplicationAddress);
    return 0;
}