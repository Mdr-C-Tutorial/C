#define _GNU_SOURCE /* syscall() 与 SYS_seccomp */
#include <errno.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

/* x32 的系统调用号在 x86-64 的编号上加了这个位 */
#define X32_SYSCALL_BIT 0x40000000

/* 装一个只拒绝一个系统调用的过滤器，命中时返回 f_errno */
static int install_filter(int syscall_nr, unsigned int t_arch, int f_errno) {
    unsigned int upper_nr_limit = 0xffffffff;

    if (t_arch == AUDIT_ARCH_X86_64) {
        upper_nr_limit = X32_SYSCALL_BIT - 1;
    }

    struct sock_filter filter[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, arch)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, t_arch, 0, 5),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
        BPF_JUMP(BPF_JMP | BPF_JGT | BPF_K, upper_nr_limit, 3, 0),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, syscall_nr, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (f_errno & SECCOMP_RET_DATA)),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
    };

    struct sock_fprog prog = {
        .len = (unsigned short)(sizeof filter / sizeof filter[0]),
        .filter = filter,
    };

    if (syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &prog) != 0) {
        perror("seccomp");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr,
                "usage: %s <syscall_nr> <arch> <errno> <prog> [<args>]\n"
                "  AUDIT_ARCH_I386    = 0x%X\n"
                "  AUDIT_ARCH_X86_64  = 0x%X\n",
                argv[0], AUDIT_ARCH_I386, AUDIT_ARCH_X86_64);
        return EXIT_FAILURE;
    }

    /* 没有 CAP_SYS_ADMIN 时，设置 no_new_privs 才能装过滤器 */
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        perror("prctl");
        return EXIT_FAILURE;
    }

    if (install_filter((int)strtol(argv[1], NULL, 0),
                       (unsigned int)strtoul(argv[2], NULL, 0),
                       (int)strtol(argv[3], NULL, 0)) != 0) {
        return EXIT_FAILURE;
    }

    execv(argv[4], &argv[4]);
    perror("execv");
    return EXIT_FAILURE;
}
