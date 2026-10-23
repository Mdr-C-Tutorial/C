/* args.c：逐个打印命令行参数，两侧加方括号以便看清空白 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = [%s]\n", i, argv[i]);
    }
    if (argc > 1 && strcmp(argv[1], "--fail") == 0) {
        return 3;
    }
    return EXIT_SUCCESS;
}
