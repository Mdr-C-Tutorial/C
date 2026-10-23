#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int g_zero = 0;
static void (*g_recurse)(int);

static void recurse(int depth)
{
    volatile char pad[1024];

    pad[0] = (char)depth;
    g_recurse(depth + 1); /* 经函数指针调用，编译器看不到直接递归 */
    (void)pad[0];
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2) {
        printf("usage: crash <av|div|stack|raise>\n");
        return 2;
    }
    if (strcmp(argv[1], "av") == 0) {
        int *p = NULL;
        printf("value=%d\n", *p);
    } else if (strcmp(argv[1], "div") == 0) {
        printf("value=%d\n", 10 / g_zero);
    } else if (strcmp(argv[1], "stack") == 0) {
        g_recurse = recurse;
        recurse(0);
    } else {
        RaiseException(0xE0000001, 0, 2, (ULONG_PTR[]){111, 222});
    }
    printf("not reached\n");
    return 0;
}