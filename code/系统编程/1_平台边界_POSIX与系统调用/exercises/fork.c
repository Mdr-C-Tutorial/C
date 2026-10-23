#include <stdio.h>
#include <unistd.h>

int main(void) {
    pid_t pid = fork();
    printf("fork 返回 %d\n", (int)pid);
    return 0;
}
