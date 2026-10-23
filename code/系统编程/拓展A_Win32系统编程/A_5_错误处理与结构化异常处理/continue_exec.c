#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

static int g_zero = 0;
static int g_filter_calls;

/* 筛选器不修复原因，只是反复交回执行 */
static int retry_filter(unsigned long code)
{
    g_filter_calls++;
    printf("filter call %d, code=0x%08lX\n", g_filter_calls, code);
    if (g_filter_calls >= 3) {
        printf("give up, execute handler\n");
        return EXCEPTION_EXECUTE_HANDLER;
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    __try {
        printf("dividing\n");
        printf("%d\n", 10 / g_zero);
        printf("after the faulting instruction\n");
    }
    __except (retry_filter(GetExceptionCode())) {
        printf("handler, filter ran %d times\n", g_filter_calls);
    }
    printf("after __except\n");
    return 0;
}