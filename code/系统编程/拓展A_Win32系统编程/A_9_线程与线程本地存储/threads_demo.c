/* threads_demo.c：C11 的 <threads.h> 版本。MinGW-w64 没有这个头文件，
 * 编译时的报错就是第 1.1 节贴出的那段。 */
#include <stdio.h>
#include <threads.h>

static int worker(void *arg) {
    (void)arg;
    puts("worker");
    return 0;
}

int main(void) {
    thrd_t t;

    thrd_create(&t, worker, NULL);
    thrd_join(t, NULL);
    return 0;
}