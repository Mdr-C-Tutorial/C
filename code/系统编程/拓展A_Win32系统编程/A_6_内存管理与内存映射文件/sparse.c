/* 保留、提交、保护属性与 VirtualQuery：sparse.exe [reserved|readonly] */
#include <windows.h>
#include <psapi.h>

#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define REGION_SIZE ((SIZE_T)1 << 30)   /* 1 GiB */
#define PAGE_OFFSET ((SIZE_T)512 << 20) /* 512 MiB */

static void print_error(const wchar_t *what, DWORD err) {
    wchar_t *msg = NULL;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                 FORMAT_MESSAGE_FROM_SYSTEM |
                                 FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL, err, 0, (wchar_t *)&msg, 0, NULL);
    while (n > 0 && (msg[n - 1] == L'\r' || msg[n - 1] == L'\n')) {
        msg[--n] = L'\0';
    }
    wprintf(L"%ls: error %lu: %ls\n", what, (unsigned long)err,
            n > 0 ? msg : L"(no message)");
    LocalFree(msg);
}

/* 打印 addr 所在区域的基址、长度、状态和保护属性 */
static void show_region(const char *label, void *addr) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(addr, &mbi, sizeof mbi) == 0) {
        printf("%-22s VirtualQuery failed: %lu\n", label,
               (unsigned long)GetLastError());
        return;
    }
    const char *state = "?";
    switch (mbi.State) {
    case MEM_FREE:
        state = "free";
        break;
    case MEM_RESERVE:
        state = "reserved";
        break;
    case MEM_COMMIT:
        state = "committed";
        break;
    }
    printf("%-22s base %p  size %8llu KiB  %-9s  protect 0x%03lX\n", label,
           mbi.BaseAddress, (unsigned long long)(mbi.RegionSize / 1024), state,
           (unsigned long)mbi.Protect);
}

/* 进程当前提交的内存总量，数字来自 GetProcessMemoryInfo 的 PagefileUsage */
static unsigned long long commit_charge(void) {
    PROCESS_MEMORY_COUNTERS pmc;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) {
        return 0;
    }
    return pmc.PagefileUsage;
}

int main(int argc, char *argv[]) {
    const char *mode = argc > 1 ? argv[1] : "normal";
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    printf("page size %lu bytes, allocation granularity %lu bytes\n",
           (unsigned long)si.dwPageSize,
           (unsigned long)si.dwAllocationGranularity);

    unsigned long long before = commit_charge();
    char *base = VirtualAlloc(NULL, REGION_SIZE, MEM_RESERVE, PAGE_NOACCESS);
    if (base == NULL) {
        print_error(L"VirtualAlloc(MEM_RESERVE)", GetLastError());
        return 1;
    }
    printf("reserved 1 GiB at %p, commit charge %llu KiB -> %llu KiB\n",
           (void *)base, before / 1024, commit_charge() / 1024);
    show_region("whole 1 GiB", base);

    char *page = base + PAGE_OFFSET;
    if (VirtualAlloc(page, si.dwPageSize, MEM_COMMIT, PAGE_READWRITE) == NULL) {
        print_error(L"VirtualAlloc(MEM_COMMIT)", GetLastError());
        VirtualFree(base, 0, MEM_RELEASE);
        return 1;
    }
    printf("committed one page at %p (512 MiB into the region), "
           "commit charge %llu KiB\n",
           (void *)page, commit_charge() / 1024);
    printf("first four bytes of the fresh page: %d %d %d %d\n", page[0],
           page[1], page[2], page[3]);
    show_region("start of the region", base);
    show_region("at 512 MiB", page);
    show_region("the page after it", page + si.dwPageSize);

    strcpy(page, "this page lives in the middle");
    printf("read back: %s\n", page);

    DWORD old_protect = 0;
    if (!VirtualProtect(page, si.dwPageSize, PAGE_READONLY, &old_protect)) {
        print_error(L"VirtualProtect", GetLastError());
    }
    printf("VirtualProtect to read-only, previous protection 0x%03lX\n",
           (unsigned long)old_protect);
    show_region("at 512 MiB", page);

    if (strcmp(mode, "reserved") == 0) {
        puts("writing to a reserved page that was never committed...");
        fflush(stdout);
        base[0] = 'x';
        puts("unreachable");
    }
    if (strcmp(mode, "readonly") == 0) {
        puts("writing to a read-only page...");
        fflush(stdout);
        page[0] = 'y';
        puts("unreachable");
    }

    /* MEM_DECOMMIT 把已提交的页退回保留状态，地址还占着 */
    if (!VirtualFree(page, si.dwPageSize, MEM_DECOMMIT)) {
        print_error(L"VirtualFree(MEM_DECOMMIT)", GetLastError());
    }
    printf("after MEM_DECOMMIT, commit charge %llu KiB\n",
           commit_charge() / 1024);
    show_region("at 512 MiB", page);

    /* MEM_RELEASE 必须把大小写成 0 */
    DWORD release_err = 0;
    BOOL released = VirtualFree(base, REGION_SIZE, MEM_RELEASE);
    if (!released) {
        release_err = GetLastError();
    }
    printf("MEM_RELEASE with a nonzero size: %s, error %lu\n",
           released ? "TRUE" : "FALSE", (unsigned long)release_err);
    if (!VirtualFree(base, 0, MEM_RELEASE)) {
        print_error(L"VirtualFree(MEM_RELEASE)", GetLastError());
        return 1;
    }
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(base, &mbi, sizeof mbi);
    printf("after MEM_RELEASE the range is %s again\n",
           mbi.State == MEM_FREE ? "free" : "not free");
    return 0;
}