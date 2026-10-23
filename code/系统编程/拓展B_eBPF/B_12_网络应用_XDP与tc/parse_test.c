/*
 * 用普通 C 程序验证 XDP 里的报文解析偏移：手工拼以太网 + IPv4 + UDP 帧，
 * 按与内核态程序相同的顺序和边界检查逐层解析。不打开套接字，不碰网络。
 *
 *   gcc -std=c17 -Wall -Wextra parse_test.c -o parse_test
 *   ./parse_test
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ETH_P_IP 0x0800
#define IPPROTO_UDP 17

/* 与 vmlinux.h 中同名的结构体，字段顺序和大小一致 */
struct ethhdr {
    unsigned char h_dest[6];
    unsigned char h_source[6];
    uint16_t h_proto;
};

struct iphdr {
    uint8_t ihl : 4;
    uint8_t version : 4;
    uint8_t tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
};

struct udphdr {
    uint16_t source;
    uint16_t dest;
    uint16_t len;
    uint16_t check;
};

static void put_be16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static uint16_t get_be16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

/* 输入一个指向帧首字节的缓冲区，返回按解析结果应当返回的 XDP 动作 */
static int parse_frame(const uint8_t *buf, size_t len, uint16_t drop_port) {
    const void *data = buf;
    const void *data_end = buf + len;
    const struct ethhdr *eth = data;
    const struct iphdr *ip;
    const struct udphdr *udp;
    size_t ip_off;
    size_t udp_off;

    if ((const void *)(eth + 1) > data_end) {
        printf("  解析失败：以太网头不完整\n");
        return 2; /* XDP_PASS */
    }
    printf("  以太网：h_proto = 0x%04x\n", get_be16((const uint8_t *)&eth->h_proto));
    if (get_be16((const uint8_t *)&eth->h_proto) != ETH_P_IP) {
        printf("  不是 IPv4，放行\n");
        return 2; /* XDP_PASS */
    }

    ip = (const struct iphdr *)(eth + 1);
    if ((const void *)(ip + 1) > data_end) {
        printf("  解析失败：固定部分 20 字节的 IPv4 头不完整\n");
        return 2;
    }
    printf("  以太网头长度 %lu 字节，IPv4 头从偏移 %lu 开始\n",
           (unsigned long)sizeof(struct ethhdr),
           (unsigned long)sizeof(struct ethhdr));
    printf("  IPv4：version = %u，ihl = %u，头长 %u 字节，protocol = %u，"
           "tot_len = %u\n",
           ip->version, ip->ihl, ip->ihl * 4, ip->protocol,
           get_be16((const uint8_t *)&ip->tot_len));
    if (ip->ihl < 5) {
        printf("  ihl 小于 5，报文头非法，放行\n");
        return 2;
    }

    ip_off = sizeof(struct ethhdr);
    udp_off = ip_off + (size_t)ip->ihl * 4;
    printf("  传输层头从偏移 %lu 开始（%lu + %u × 4）\n",
           (unsigned long)udp_off, (unsigned long)ip_off, ip->ihl);
    if (ip->protocol != IPPROTO_UDP) {
        printf("  协议不是 UDP，放行\n");
        return 2;
    }

    udp = (const struct udphdr *)((const uint8_t *)ip + ip->ihl * 4);
    if ((const void *)udp + sizeof(*udp) > data_end) {
        printf("  解析失败：UDP 头不完整\n");
        return 2;
    }
    printf("  UDP：源端口 %u，目的端口 %u，长度 %u\n",
           get_be16((const uint8_t *)&udp->source),
           get_be16((const uint8_t *)&udp->dest),
           get_be16((const uint8_t *)&udp->len));
    if (get_be16((const uint8_t *)&udp->dest) == drop_port) {
        printf("  目的端口等于 %u，返回 XDP_DROP\n", drop_port);
        return 1; /* XDP_DROP */
    }
    printf("  目的端口不等于 %u，返回 XDP_PASS\n", drop_port);
    return 2; /* XDP_PASS */
}

/* 拼一帧：以太网 14 字节 + IPv4 头（20 字节或带 4 字节选项）+ UDP 头 +
 * 4 字节负载 */
static size_t build_frame(uint8_t *frame, size_t cap, int with_options,
                          uint16_t dest_port) {
    size_t eth_len = 14;
    size_t ip_len = with_options ? 24 : 20;
    size_t udp_len = 8;
    size_t payload_len = 4;

    memset(frame, 0, cap);
    /* 目的 MAC 与源 MAC 留全零，不影响解析 */
    put_be16(frame + 12, ETH_P_IP);

    uint8_t *ip = frame + eth_len;
    ip[0] = (uint8_t)((4u << 4) | (with_options ? 6u : 5u)); /* version, ihl */
    ip[1] = 0;                                               /* tos */
    put_be16(ip + 2, (uint16_t)(ip_len + udp_len + payload_len));
    put_be16(ip + 4, 0x1234); /* id */
    put_be16(ip + 6, 0);      /* flags 与片偏移 */
    ip[8] = 64;               /* ttl */
    ip[9] = IPPROTO_UDP;      /* protocol */
    put_be16(ip + 10, 0);     /* 校验和，解析时不用 */
    /* 源地址 127.0.0.1，目的地址 127.0.0.1 */
    ip[12] = 127;
    ip[15] = 1;
    ip[16] = 127;
    ip[19] = 1;
    if (with_options) {
        memset(ip + 20, 0, 4); /* 4 字节选项，全零（NOP） */
    }

    uint8_t *udp = ip + ip_len;
    put_be16(udp + 0, 40000); /* 源端口 */
    put_be16(udp + 2, dest_port);
    put_be16(udp + 4, (uint16_t)(udp_len + payload_len));
    put_be16(udp + 6, 0);

    memcpy(udp + udp_len, "test", payload_len);
    return eth_len + ip_len + udp_len + payload_len;
}

int main(void) {
    uint8_t frame[128];
    size_t len;
    int action;

    printf("第一帧：20 字节的 IPv4 头，目的端口 20001\n");
    len = build_frame(frame, sizeof frame, 0, 20001);
    printf("  帧长 %lu 字节\n", (unsigned long)len);
    action = parse_frame(frame, len, 20001);
    printf("  返回值：%d\n\n", action);

    printf("第二帧：带 4 字节选项的 IPv4 头（ihl = 6），目的端口 20002\n");
    len = build_frame(frame, sizeof frame, 1, 20002);
    printf("  帧长 %lu 字节\n", (unsigned long)len);
    action = parse_frame(frame, len, 20001);
    printf("  返回值：%d\n", action);
    return 0;
}