/* 保留、提交、访问三步对提交量与工作集的影响 */
#include <windows.h>
#include <psapi.h>

#include <stdio.h>

static void show(const char *label) {
    PROCESS_MEMORY_COUNTERS pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc);
    printf("%-28s PagefileUsage %8llu KiB  WorkingSetSize %8llu KiB\n", label,
           (unsigned long long)(pmc.PagefileUsage / 1024),
           (unsigned long long)(pmc.WorkingSetSize / 1024));
}

int main(void) {
    show("start");
    char *p = VirtualAlloc(NULL, (SIZE_T)256 << 20, MEM_RESERVE, PAGE_NOACCESS);
    if (p == NULL) {
        printf("VirtualAlloc(MEM_RESERVE) failed: %lu\n",
               (unsigned long)GetLastError());
        return 1;
    }
    show("after reserve 256 MiB");
    if (VirtualAlloc(p, (SIZE_T)64 << 20, MEM_COMMIT, PAGE_READWRITE) == NULL) {
        printf("VirtualAlloc(MEM_COMMIT) failed: %lu\n",
               (unsigned long)GetLastError());
        VirtualFree(p, 0, MEM_RELEASE);
        return 1;
    }
    show("after commit 64 MiB");
    for (SIZE_T i = 0; i < ((SIZE_T)64 << 20); i += 4096) {
        p[i] = 1;
    }
    show("after touching 64 MiB");
    VirtualFree(p, 0, MEM_RELEASE);
    show("after release");
    return 0;
}