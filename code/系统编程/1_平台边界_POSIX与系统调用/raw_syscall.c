#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

/* 只适用于 x86-64 Linux：自己执行 syscall 指令，不经过 glibc 的包装函数 */
static long raw_write(int fd, const void *buf, long count) {
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"((long)SYS_write), "D"((long)fd), "S"(buf),
                       "d"(count)
                     : "rcx", "r11", "memory");
    return ret;
}

int main(void) {
    static const char msg[] = "hello from a raw syscall\n";
    long ok = raw_write(STDOUT_FILENO, msg, (long)(sizeof msg - 1));
    errno = 0;
    long bad = raw_write(-1, msg, (long)(sizeof msg - 1));
    int err = errno; /* 没有包装函数，errno 不会被改动 */
    printf("有效描述符返回 %ld，无效描述符返回 %ld，errno = %d\n", ok, bad, err);
    return 0;
}
