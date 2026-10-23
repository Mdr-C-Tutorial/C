#define _POSIX_C_SOURCE 200809L

/*
 * watchdog_keepalive：硬件看门狗的喂狗片段。
 *
 * 打开 /dev/watchdog 就开始计时；程序每完成一轮工作后调用一次 watchdog_feed()，
 * 退出时写入 magic 字符 'V' 关闭看门狗，否则设备会一直重启。
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/watchdog.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct watchdog {
    int fd;
};

static int watchdog_open(struct watchdog *wd, const char *path, int timeout_s) {
    wd->fd = open(path, O_WRONLY | O_CLOEXEC);
    if (wd->fd < 0) {
        return -1;
    }
    if (timeout_s > 0) {
        int value = timeout_s;

        if (ioctl(wd->fd, WDIOC_SETTIMEOUT, &value) < 0) {
            /* 驱动可以拒绝这个值，超时保持原样。 */
        }
    }
    return 0;
}

static int watchdog_feed(struct watchdog *wd) {
    static const char ping = 'k';

    return write(wd->fd, &ping, 1) == 1 ? 0 : -1;
}

static void watchdog_close(struct watchdog *wd) {
    if (wd->fd >= 0) {
        static const char magic = 'V';

        if (write(wd->fd, &magic, 1) != 1) {
            /* 写不进去就无法关闭，让设备按超时复位。 */
        }
        close(wd->fd);
        wd->fd = -1;
    }
}

int main(void) {
    struct watchdog wd = {.fd = -1};

    if (watchdog_open(&wd, "/dev/watchdog", 10) < 0) {
        fprintf(stderr, "watchdog: %s\n", strerror(errno));
        return 1;
    }
    /* 工作循环：这里用 sleep 冒充一轮采集与处理。 */
    for (int round = 0; round < 3; round++) {
        sleep(1);
        if (watchdog_feed(&wd) < 0) {
            fprintf(stderr, "feed: %s\n", strerror(errno));
            return 1;
        }
        printf("round %d done, watchdog fed\n", round + 1);
    }
    /* 正常收尾时显式关闭，不给设备复位。 */
    watchdog_close(&wd);
    printf("watchdog closed\n");
    return 0;
}