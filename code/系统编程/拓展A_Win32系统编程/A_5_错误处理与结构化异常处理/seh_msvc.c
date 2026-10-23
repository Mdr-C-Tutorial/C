#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

/* 用筛选器打印异常码，再按码决定是否处理 */
static int filter(unsigned long code)
{
    printf("filter: code=0x%08lX\n", code);
    if (code == EXCEPTION_ACCESS_VIOLATION) {
        return EXCEPTION_EXECUTE_HANDLER;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(void)
{
    int *p = NULL;
    int value = 0;

    __try {
        value = *p; /* 访问空指针，由硬件引发访问违例 */
    }
    __except (filter(GetExceptionCode())) {
        printf("handled access violation\n");
    }

    printf("still running, value=%d\n", value);
    return 0;
}