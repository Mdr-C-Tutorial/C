#include <stdio.h>
#include <sys/wait.h>

int main(void) {
    int status;
    wait(&status);
    return 0;
}
