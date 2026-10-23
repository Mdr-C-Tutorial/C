#include <stdio.h>
#include <unistd.h>

int main(void) {
    static const char msg[] = "hello from write\n";
    if (write(1, msg, sizeof msg - 1) == -1) {
        perror("write");
        return 1;
    }
#ifdef _POSIX_VERSION
    printf("_POSIX_VERSION = %ld\n", (long)_POSIX_VERSION);
#else
    printf("_POSIX_VERSION 未定义\n");
#endif
    printf("pid = %d\n", (int)getpid());
    return 0;
}
