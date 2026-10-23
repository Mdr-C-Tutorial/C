#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* 无操作系统模式：只启用回调风格的 raw API */
#define NO_SYS                          1
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0
#define SYS_LIGHTWEIGHT_PROT            0

/* 启用回环接口 127.0.0.1，用于单进程自发自收测试 */
#define LWIP_NETIF_LOOPBACK             1
#define LWIP_HAVE_LOOPIF                1
#define LWIP_LOOPBACK_MAX_PBUFS         8

/* 协议开关 */
#define LWIP_IPV4                       1
#define LWIP_ICMP                       1
#define LWIP_RAW                        0
#define LWIP_UDP                        1
#define LWIP_TCP                        1
#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define LWIP_DHCP                       0

/* 内存与缓冲区预算 */
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        4096
#define PBUF_POOL_SIZE                  12
#define PBUF_POOL_BUFSIZE               592

#define TCP_MSS                         536
#define TCP_WND                         (2 * TCP_MSS)
#define TCP_SND_BUF                     (2 * TCP_MSS)
#define MEMP_NUM_TCP_PCB                4
#define MEMP_NUM_TCP_PCB_LISTEN         2
#define MEMP_NUM_TCP_SEG                12

#endif /* LWIPOPTS_H */
