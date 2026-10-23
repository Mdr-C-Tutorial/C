#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

/* 筛选器里取异常信息：访问类型与地址 */
static int av_filter(PEXCEPTION_POINTERS info)
{
    EXCEPTION_RECORD *rec = info->ExceptionRecord;

    if (rec->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    printf("access violation: %ls at %p, rip=%p\n",
           rec->ExceptionInformation[0] == 0 ? L"read" : L"write",
           (void *)rec->ExceptionInformation[1],
           (void *)info->ContextRecord->Rip);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(void)
{
    volatile int *p = (int *)0x10;
    int value = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    __try {
        *p = 42;
        value = *p;
    }
    __except (av_filter(GetExceptionInformation())) {
        printf("handled, value=%d\n", value);
    }
    return 0;
}