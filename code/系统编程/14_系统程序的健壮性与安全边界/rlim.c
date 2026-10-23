#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <sys/resource.h>

static void show(const char *name, int resource) {
    struct rlimit rl;
    if (getrlimit(resource, &rl) == -1) {
        perror("getrlimit");
        return;
    }
    if (rl.rlim_cur == RLIM_INFINITY) {
        printf("%-14s  soft=unlimited  hard=unlimited\n", name);
    } else if (rl.rlim_max == RLIM_INFINITY) {
        printf("%-14s  soft=%-10llu  hard=unlimited\n", name,
               (unsigned long long)rl.rlim_cur);
    } else {
        printf("%-14s  soft=%-10llu  hard=%llu\n", name,
               (unsigned long long)rl.rlim_cur,
               (unsigned long long)rl.rlim_max);
    }
}

int main(void) {
    show("RLIMIT_NOFILE", RLIMIT_NOFILE);
    show("RLIMIT_CORE", RLIMIT_CORE);
    show("RLIMIT_AS", RLIMIT_AS);
    show("RLIMIT_STACK", RLIMIT_STACK);
    return 0;
}