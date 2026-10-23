#define _POSIX_C_SOURCE 200809L

/*
 * notify_demo：在普通 shell 里验证 notify_send()。
 *
 * 用法：先启动一个数据报接收端，再把它的路径设为 $NOTIFY_SOCKET。
 *
 *   python3 -c 'import socket,os; s=socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM); s.bind("notify.sock"); print(s.recv(64))' &
 *   NOTIFY_SOCKET=notify.sock ./notify_demo
 */

#include "sdnotify.h"

#include <stdio.h>

int main(void) {
    if (notify_send("READY=1\nSTATUS=sensord started") < 0) {
        fprintf(stderr, "notify_send: NOTIFY_SOCKET 未设置或发送失败\n");
        return 1;
    }
    puts("notification sent");
    return 0;
}
