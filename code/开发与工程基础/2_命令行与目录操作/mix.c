/* mix.c：先写标准输出并刷新，再写标准错误，用来观察重定向顺序 */
#include <stdio.h>

int main(void) {
    printf("stdout line\n");
    fflush(stdout);
    fprintf(stderr, "stderr line\n");
    return 0;
}
