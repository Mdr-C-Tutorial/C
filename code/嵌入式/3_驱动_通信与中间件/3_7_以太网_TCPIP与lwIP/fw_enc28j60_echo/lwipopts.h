#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* 1. 无操作系统模式：只使用回调式 raw API */
#define NO_SYS                          1
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0
#define SYS_LIGHTWEIGHT_PROT            0

/* 2. 协议裁剪：保留 IPv4、ARP、ICMP、UDP、TCP、DHCP */
#define LWIP_IPV4                       1
#define LWIP_IPV6                       0
#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define LWIP_ICMP                       1
#define LWIP_RAW                        0
#define LWIP_UDP                        1
#define LWIP_TCP                        1
#define LWIP_DHCP                       1
#define LWIP_DHCP_DOES_ACD_CHECK        0
#define LWIP_DNS                        0
#define LWIP_IGMP                       0
#define LWIP_AUTOIP                     0
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_STATS                      0

/* 3. 内存对齐（Cortex-M3 为 4 字节对齐） */
#define MEM_ALIGNMENT                   4

#ifdef LWIP_PROFILE_LARGE
/* 宽松配置（用于第 9 节与习题 60372 的 SRAM 占用对比） */
#define MEM_SIZE                        6144
#define PBUF_POOL_SIZE                  16
#define PBUF_POOL_BUFSIZE               592
#define TCP_MSS                         536
#define TCP_WND                         (4 * TCP_MSS)
#define TCP_SND_BUF                     (4 * TCP_MSS)
#define MEMP_NUM_TCP_PCB                8
#define MEMP_NUM_TCP_PCB_LISTEN         4
#define MEMP_NUM_TCP_SEG                16
#define MEMP_NUM_PBUF                   16
#define MEMP_NUM_UDP_PCB                4
#define ARP_TABLE_SIZE                  8
#else
/* 面向 20 KiB SRAM（STM32F103C8T6）的紧凑配置 */
#define MEM_SIZE                        1600
#define PBUF_POOL_SIZE                  6
#define PBUF_POOL_BUFSIZE               592
#define TCP_MSS                         536
#define TCP_WND                         (2 * TCP_MSS)
#define TCP_SND_BUF                     (2 * TCP_MSS)
#define MEMP_NUM_TCP_PCB                4
#define MEMP_NUM_TCP_PCB_LISTEN         2
#define MEMP_NUM_TCP_SEG                8
#define MEMP_NUM_PBUF                   8
#define MEMP_NUM_UDP_PCB                2
#define ARP_TABLE_SIZE                  4
#endif

#endif /* LWIPOPTS_H */
