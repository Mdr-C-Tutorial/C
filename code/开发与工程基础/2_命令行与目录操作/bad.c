/* bad.c：故意留下两处错误，只用于演示编译诊断 */
#include <stdio.h>

int main(void) {
    int total = count + 1;
    printf("%d\n", total)
    return 0;
}
