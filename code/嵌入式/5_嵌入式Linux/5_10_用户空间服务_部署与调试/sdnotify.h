#ifndef SDNOTIFY_H
#define SDNOTIFY_H

/* 向 systemd 的服务管理器发送一条状态消息，如 "READY=1"、"WATCHDOG=1"。 */
int notify_send(const char *state);

#endif
