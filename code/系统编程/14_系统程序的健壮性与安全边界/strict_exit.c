/* 严格模式 seccomp 下用 _exit(0) 退出：glibc 的 _exit 调用 exit_group，
 * 这个系统调用不在严格模式的允许列表里。 */
#define _GNU_SOURCE
#include <linux/seccomp.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <unistd.h>

int main(void) {
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT) == -1) {
        perror("prctl");
        return 1;
    }
    static const char msg[] = "about to call _exit(0)\n";
    write(STDOUT_FILENO, msg, sizeof msg - 1);
    _exit(0);
}