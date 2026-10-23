#define _POSIX_C_SOURCE 200809L

/*
 * sdnotify：不链接 libsystemd，直接向 $NOTIFY_SOCKET 发数据报。
 *
 * 协议见 sd_notify(3)：消息体是一串 "KEY=VALUE\n"；READY=1 表示启动完成，
 * WATCHDOG=1 表示该喂狗了。套接字由服务管理器创建，地址以 '@' 开头时
 * 表示抽象命名空间。
 */

#include "sdnotify.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int notify_send(const char *state) {
    const char *path = getenv("NOTIFY_SOCKET");

    if (path == NULL || *path == '\0' || state == NULL) {
        return -1;
    }
    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);

    if (fd < 0) {
        return -1;
    }
    struct sockaddr_un addr;
    size_t skip = path[0] == '@' ? 1 : 0;
    size_t len = strlen(path + skip);

    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    if (len + 1 > sizeof addr.sun_path) {
        close(fd);
        return -1;
    }
    memcpy(addr.sun_path + skip, path + skip, len);
    /* 抽象命名空间：地址的第一个字节是 0，长度从 sun_path 起算。 */
    socklen_t addr_len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + skip + len);

    ssize_t sent = sendto(fd, state, strlen(state), 0, (struct sockaddr *)&addr,
                          addr_len);

    close(fd);
    return sent == (ssize_t)strlen(state) ? 0 : -1;
}