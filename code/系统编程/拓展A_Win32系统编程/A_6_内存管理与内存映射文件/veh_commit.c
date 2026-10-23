/* 用向量化异常处理实现「访问时按需提交」：veh_commit.c */
#include <windows.h>

#include <stdio.h>

#define PAGE_COUNT 16

static char *g_reserved = NULL;
static SIZE_T g_page_size = 0;
static int g_commits = 0;

/* 写一页就提交一页，然后让出错的指令重新执行 */
static LONG CALLBACK commit_on_demand(EXCEPTION_POINTERS *info) {
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    /* ExceptionInformation[1] 才是出错的那个地址；[0] 是访问类型 */
    char *fault = (char *)info->ExceptionRecord->ExceptionInformation[1];
    if (fault < g_reserved || fault >= g_reserved + PAGE_COUNT * g_page_size) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(fault, &mbi, sizeof mbi) == 0 ||
        mbi.State != MEM_RESERVE) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (VirtualAlloc(mbi.BaseAddress, g_page_size, MEM_COMMIT, PAGE_READWRITE) ==
        NULL) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    g_commits++;
    printf("  handler: committed the page at %p, retrying the instruction\n",
           mbi.BaseAddress);
    return EXCEPTION_CONTINUE_EXECUTION;
}

int main(void) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    g_page_size = si.dwPageSize;

    g_reserved = VirtualAlloc(NULL, PAGE_COUNT * g_page_size, MEM_RESERVE,
                              PAGE_NOACCESS);
    if (g_reserved == NULL) {
        printf("VirtualAlloc failed: %lu\n", (unsigned long)GetLastError());
        return 1;
    }

    PVOID vh = AddVectoredExceptionHandler(1, commit_on_demand);
    if (vh == NULL) {
        printf("AddVectoredExceptionHandler failed: %lu\n",
               (unsigned long)GetLastError());
        VirtualFree(g_reserved, 0, MEM_RELEASE);
        return 1;
    }

    for (int i = 0; i < 4; i++) {
        char *p = g_reserved + i * g_page_size;
        printf("writing to page %d at %p\n", i, (void *)p);
        *p = (char)('a' + i);
        printf("  value read back: %c\n", *p);
    }
    printf("pages committed by the handler: %d\n", g_commits);

    RemoveVectoredExceptionHandler(vh);
    VirtualFree(g_reserved, 0, MEM_RELEASE);
    return 0;
}