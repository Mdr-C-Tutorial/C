#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

/* 退出途经的三种情况：正常走完、return、goto */
static void finally_on_paths(int mode)
{
    __try {
        if (mode == 1) {
            printf("  returning from inside __try\n");
            return;
        }
        if (mode == 2) {
            printf("  jumping out with goto\n");
            goto out;
        }
        printf("  normal exit of __try\n");
    }
    __finally {
        printf("  finally, abnormal=%d\n", AbnormalTermination());
    }
out:
    printf("  after the guarded section\n");
}

int main(void)
{
    printf("mode 0:\n");
    finally_on_paths(0);
    printf("mode 1 (return):\n");
    finally_on_paths(1);
    printf("mode 2 (goto):\n");
    finally_on_paths(2);
    return 0;
}