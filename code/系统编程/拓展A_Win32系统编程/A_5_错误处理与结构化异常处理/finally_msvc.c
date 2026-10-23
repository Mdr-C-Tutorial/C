#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

static int divide(int a, int b)
{
    int r = 0;

    __try {
        r = a / b;
        if (r > 100) {
            __leave; /* 直接跳到 __finally */
        }
        printf("  normal path, r=%d\n", r);
    }
    __finally {
        printf("  finally (abnormal=%d)\n", AbnormalTermination());
    }
    return r;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("normal division:\n");
    printf("  result=%d\n", divide(10, 2));

    printf("divide by zero:\n");
    __try {
        printf("  result=%d\n", divide(10, 0));
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("  caught 0x%08lX\n", GetExceptionCode());
    }

    printf("__leave path:\n");
    printf("  result=%d\n", divide(1000, 2));
    return 0;
}