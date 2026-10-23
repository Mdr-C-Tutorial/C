#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    static const size_t sizes[] = {1000, 1024 * 1024, 1024 * 1024};

    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        char *p = malloc(sizes[i]);
        if (p == NULL) {
            return EXIT_FAILURE;
        }
        p[0] = 'x'; /* 碰一下，避免只看到分配、看不到实际使用 */
        printf("malloc(%zu) = %p\n", sizes[i], (void *)p);
        free(p);
        printf("free 完成\n");
    }
    return 0;
}
