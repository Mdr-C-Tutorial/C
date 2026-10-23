#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    int n = strlen(argv[0]);
    unsigned char small = argc + 300;
    printf("%d %u\n", n, (unsigned)small);
    return 0;
}
