/* 降权示例：只在启动时以 root 运行才有意义，普通用户下编译通过、
 * 运行时会因 setgroups 失败而提前退出。顺序：setgroups → setgid → setuid，
 * 每一步之后检查实际身份，最后用 getuid/geteuid 确认无法再回退。 */
#define _GNU_SOURCE /* setgroups、setresgid、setresuid */
#include <errno.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static void die(const char *what) {
    fprintf(stderr, "%s: %s\n", what, strerror(errno));
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s UID GID\n", argv[0]);
        return EXIT_FAILURE;
    }
    uid_t uid = (uid_t)strtoul(argv[1], NULL, 10);
    gid_t gid = (gid_t)strtoul(argv[2], NULL, 10);

    /* 1. 附加组必须先清空。setuid 之后就没有权限再调用 setgroups 了。 */
    if (setgroups(0, NULL) == -1) {
        die("setgroups");
    }
    /* 2. 组身份。setgid 之后，setuid 前仍持有特权，但顺序不能反。 */
    if (setresgid(gid, gid, gid) == -1) {
        die("setresgid");
    }
    /* 3. 用户身份。保存的设置用户 ID 也一并改掉，防止程序后面
     *    通过 seteuid(0) 把特权拿回来。 */
    if (setresuid(uid, uid, uid) == -1) {
        die("setresuid");
    }

    /* 4. 检查确实降下来了：三个用户 ID 都应当是目标值，且不能再切回 0。 */
    if (getuid() != uid || geteuid() != uid) {
        fprintf(stderr, "privileges not dropped\n");
        return EXIT_FAILURE;
    }
    if (seteuid(0) == 0) {
        fprintf(stderr, "still able to regain root\n");
        return EXIT_FAILURE;
    }
    printf("dropped to uid=%lu gid=%lu\n", (unsigned long)getuid(),
           (unsigned long)getgid());
    return EXIT_SUCCESS;
}