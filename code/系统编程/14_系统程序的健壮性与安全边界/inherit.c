/* 演示进程从创建者那里继承的状态：umask、当前目录、信号掩码、
 * 资源限制。父进程先改掉这几样，再 exec 同一个程序的 child 模式，
 * 由子进程打印自己看到的值。 */
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

static void show_mask(void) {
    sigset_t set;
    sigemptyset(&set);
    if (sigprocmask(SIG_SETMASK, NULL, &set) == -1) {
        perror("sigprocmask");
        return;
    }
    printf("blocked:");
    int any = 0;
    for (int sig = 1; sig < 32; sig++) {
        if (sigismember(&set, sig) == 1) {
            printf(" %d", sig);
            any = 1;
        }
    }
    if (!any) {
        printf(" (none)");
    }
    printf("\n");
}

static void show_limits(void) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("getrlimit");
        return;
    }
    printf("RLIMIT_NOFILE soft=%llu\n", (unsigned long long)rl.rlim_cur);
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "child") == 0) {
        char cwd[256];
        if (getcwd(cwd, sizeof cwd) == NULL) {
            perror("getcwd");
            return 1;
        }
        const char *base = strrchr(cwd, '/');
        base = base == NULL ? cwd : base + 1;
        mode_t mask = umask(0);
        umask(mask);
        printf("child: umask=%03o cwd ends with %s\n", mask, base);
        show_mask();
        show_limits();
        return 0;
    }

    if (mkdir("subdir", 0755) == -1 && access("subdir", F_OK) != 0) {
        perror("mkdir");
        return 1;
    }
    umask(077);
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        perror("sigprocmask");
        return 1;
    }
    struct rlimit rl = {.rlim_cur = 64, .rlim_max = 64};
    if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("setrlimit");
        return 1;
    }
    char exe[256];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n < 0) {
        perror("readlink");
        return 1;
    }
    exe[n] = '\0';
    if (chdir("subdir") == -1) {
        perror("chdir");
        return 1;
    }
    char *new_argv[] = {exe, "child", NULL};
    execv(exe, new_argv);
    perror("execv");
    return 1;
}