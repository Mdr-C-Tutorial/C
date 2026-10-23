#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>

/* noinline 与空汇编配合，保证函数在目标文件里有一个真实的符号，
 * 不会被编译器内联进 main。附加 uprobe 需要这样一个符号。 */
__attribute__((noinline)) int uprobed_add(int a, int b)
{
    __asm__ volatile("");
    return a + b;
}

__attribute__((noinline)) int uprobed_sub(int a, int b)
{
    __asm__ volatile("");
    return a - b;
}

int main(void)
{
    int i;

    for (i = 0; i < 3; i++) {
        printf("uprobed_add(%d, %d) = %d\n", i, i + 1, uprobed_add(i, i + 1));
        printf("uprobed_sub(%d, %d) = %d\n", i, i, uprobed_sub(i, i));
        sleep(1);
    }
    return 0;
}