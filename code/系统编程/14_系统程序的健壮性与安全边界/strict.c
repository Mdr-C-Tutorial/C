/* 严格模式的 seccomp：只允许 read、write、_exit、sigreturn。
 * 正常退出必须用 syscall(SYS_exit, 0)，因为 glibc 的 exit/_exit
 * 走的是 exit_group。 */
#define _GNU_SOURCE /* syscall、SYS_getpid 与 linux/seccomp.h */
#include <errno.h>
#include <linux/seccomp.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    (void)argv;
    int call_getpid = argc > 1;
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT) == -1) {
        perror("prctl");
        return 1;
    }
    static const char msg[] = "strict mode: only read/write/_exit/sigreturn\n";
    write(STDOUT_FILENO, msg, sizeof msg - 1);
    if (call_getpid) {
        /* 这一步会被内核杀死，进程收不到返回值 */
        syscall(SYS_getpid);
    }
    syscall(SYS_exit, 0);
}