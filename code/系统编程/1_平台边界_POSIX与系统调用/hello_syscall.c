#define _GNU_SOURCE /* syscall() 是 GNU 扩展 */
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(void) {
    static const char msg[] = "hello, world\n";
    long n = syscall(SYS_write, STDOUT_FILENO, msg, sizeof msg - 1);
    if (n == -1) {
        perror("syscall");
        return 1;
    }
    return 0;
}
