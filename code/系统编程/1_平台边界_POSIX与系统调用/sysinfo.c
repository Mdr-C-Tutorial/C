#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 返回 -1 且 errno 没有变化，表示这一项没有确定的值 */
static long query(int name, const char *what) {
    errno = 0;
    long value = sysconf(name);
    if (value == -1 && errno != 0) {
        die(what);
    }
    return value;
}

int main(void) {
    struct utsname u;
    if (uname(&u) == -1) {
        die("uname");
    }
    printf("系统：%s %s %s\n", u.sysname, u.release, u.machine);

    printf("页大小：%ld 字节\n", query(_SC_PAGESIZE, "sysconf(_SC_PAGESIZE)"));
    printf("可打开的文件数上限：%ld\n",
           query(_SC_OPEN_MAX, "sysconf(_SC_OPEN_MAX)"));
    printf("在线 CPU 数：%ld\n",
           query(_SC_NPROCESSORS_ONLN, "sysconf(_SC_NPROCESSORS_ONLN)"));
    printf("每秒时钟滴答数：%ld\n", query(_SC_CLK_TCK, "sysconf(_SC_CLK_TCK)"));

    char libc[64];
    if (confstr(_CS_GNU_LIBC_VERSION, libc, sizeof libc) == 0) {
        die("confstr");
    }
    printf("C 库：%s\n", libc);
    printf("POSIX 版本：%ld\n", (long)_POSIX_VERSION);
    return 0;
}
