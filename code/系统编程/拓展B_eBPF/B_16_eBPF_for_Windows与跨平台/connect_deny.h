/* 两个平台共用的部分：策略常量。
 *
 * 头文件不包含任何平台头，两边都能直接包含。
 */
#ifndef CONNECT_DENY_H
#define CONNECT_DENY_H

/* 拒绝的目标，地址按主机字节序写，端口按主机字节序写 */
#define DENY_ADDR 0x7f000001u /* 127.0.0.1 */
#define DENY_PORT 20002

#endif
