#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

static int g_zero = 0;

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* 内层筛选器拒绝处理，外层接着找 */
    __try {
        __try {
            printf("inner try\n");
            printf("%d\n", 10 / g_zero);
        }
        __except (printf("inner filter: code=0x%08lX\n", GetExceptionCode()),
                  EXCEPTION_CONTINUE_SEARCH) {
            printf("inner handler (never runs)\n");
        }
        printf("after inner (never runs)\n");
    }
    __except (printf("outer filter: code=0x%08lX\n", GetExceptionCode()),
              EXCEPTION_EXECUTE_HANDLER) {
        printf("outer handler: code=0x%08lX\n", GetExceptionCode());
    }

    /* 嵌套的 __try/__finally 在异常展开时的顺序 */
    __try {
        __try {
            __try {
                printf("raise inside two __finally blocks\n");
                RaiseException(0xE0000002, 0, 0, NULL);
            }
            __finally {
                printf("inner finally\n");
            }
        }
        __finally {
            printf("outer finally\n");
        }
    }
    __except (printf("outer filter (second block): code=0x%08lX\n",
                     GetExceptionCode()),
              EXCEPTION_EXECUTE_HANDLER) {
        printf("caught 0x%08lX\n", GetExceptionCode());
    }
    printf("after the last __except\n");
    return 0;
}