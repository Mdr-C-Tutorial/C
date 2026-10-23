#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int g_counter = 1;

/* 带参数运行时把 g_counter 设成参数值，并在最后多停一秒，
 * 这样同时启动两个进程时，两边的输出能对得上 */
int main(int argc, char *argv[]) {
    int local = 2;
    void *heap = malloc(16);
    if (heap == NULL) {
        return EXIT_FAILURE;
    }
    if (argc > 1) {
        g_counter = atoi(argv[1]);
    }

    printf("&g_counter = %p（值 %d）\n", (void *)&g_counter, g_counter);
    printf("heap       = %p\n", heap);
    printf("&local     = %p\n", (void *)&local);

    if (argc > 1) {
        sleep(1);
    }
    free(heap);
    return 0;
}
