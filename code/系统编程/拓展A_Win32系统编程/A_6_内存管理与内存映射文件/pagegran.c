/* 分配粒度：一次 4 KiB 的保留实际占掉 64 KiB 地址空间 */
#include <windows.h>

#include <stdint.h>
#include <stdio.h>

static unsigned long state_of(void *addr) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(addr, &mbi, sizeof mbi) == 0) {
        return 0;
    }
    return (unsigned long)mbi.State;
}

int main(void) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    char *a = VirtualAlloc(NULL, 4096, MEM_RESERVE, PAGE_NOACCESS);
    printf("granularity %lu bytes, a 4 KiB reservation at %p\n",
           (unsigned long)si.dwAllocationGranularity, (void *)a);
    printf("aligned to the granularity: %s\n",
           (unsigned long long)(uintptr_t)a % si.dwAllocationGranularity == 0
               ? "yes"
               : "no");

    void *b = VirtualAlloc(a + 4096, 4096, MEM_RESERVE, PAGE_NOACCESS);
    printf("reserving 4 KiB at a + 4096: %s, error %lu\n",
           b == NULL ? "NULL" : "ok", (unsigned long)GetLastError());
    printf("state of that page: 0x%04lX (0x10000 is MEM_FREE)\n",
           state_of(a + 4096));

    char *c = VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    char *d = VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    printf("two 4 KiB reserve+commit blocks at %p and %p, distance %llu\n",
           (void *)c, (void *)d, (unsigned long long)((char *)d - (char *)c));

    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(c, &mbi, sizeof mbi);
    printf("the first block: %llu KiB committed, the next %llu KiB free\n",
           (unsigned long long)(mbi.RegionSize / 1024),
           (unsigned long long)(65536 - mbi.RegionSize) / 1024);

    VirtualFree(d, 0, MEM_RELEASE);
    VirtualFree(c, 0, MEM_RELEASE);
    VirtualFree(a, 0, MEM_RELEASE);
    return 0;
}