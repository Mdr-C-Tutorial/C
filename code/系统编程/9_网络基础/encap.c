/*
 * 从零拼一个 UDP over IPv4 over Ethernet 帧，打印各层的偏移和最终的字节转储。
 *
 *   gcc -std=c17 -Wall -Wextra -g encap.c -o encap
 *   ./encap
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t sum16(const uint8_t *p, size_t n) {
    uint32_t sum = 0;

    for (size_t i = 0; i + 1 < n; i += 2) {
        sum += ((uint32_t)p[i] << 8) | p[i + 1];
    }
    if (n % 2 != 0) {
        sum += (uint32_t)p[n - 1] << 8;
    }
    while (sum > 0xffffu) {
        sum = (sum & 0xffffu) + (sum >> 16);
    }
    return sum;
}

static uint16_t checksum16(const uint8_t *p, size_t n) {
    return (uint16_t)~sum16(p, n);
}

static void put_be16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

int main(void) {
    uint8_t frame[64];
    const char *text = "hi";
    size_t text_len = strlen(text);
    const size_t eth_len = 14;
    const size_t ip_len = 20;
    const size_t udp_len = 8;
    const size_t payload_off = eth_len + ip_len + udp_len;
    size_t total = payload_off + text_len;

    memset(frame, 0, sizeof frame);

    /* 以太网首部：目的地址、源地址、类型 0x0800 表示后面是 IPv4 */
    memcpy(frame, "\x02\x00\x00\x00\x00\x01", 6);
    memcpy(frame + 6, "\x02\x00\x00\x00\x00\x02", 6);
    put_be16(frame + 12, 0x0800);

    /* IPv4 首部：版本 4、首部 20 字节、总长、TTL、协议号 17 表示 UDP */
    uint8_t *ip = frame + eth_len;
    ip[0] = 0x45;
    put_be16(ip + 2, (uint16_t)(ip_len + udp_len + text_len));
    ip[8] = 64;
    ip[9] = 17;
    ip[12] = 192; ip[13] = 168; ip[14] = 1; ip[15] = 10;
    ip[16] = 192; ip[17] = 168; ip[18] = 1; ip[19] = 20;
    put_be16(ip + 10, checksum16(ip, ip_len));

    /* UDP 首部：源端口、目的端口、长度、校验和（先置 0） */
    uint8_t *udp = ip + ip_len;
    put_be16(udp, 55000);
    put_be16(udp + 2, 21000);
    put_be16(udp + 4, (uint16_t)(udp_len + text_len));
    put_be16(udp + 6, 0);

    memcpy(frame + payload_off, text, text_len);

    /* UDP 校验和覆盖伪首部（源地址、目的地址、协议号、UDP 长度）加整个 UDP 报文 */
    uint8_t pseudo[12];
    memcpy(pseudo, ip + 12, 8);
    pseudo[8] = 0;
    pseudo[9] = 17;
    put_be16(pseudo + 10, (uint16_t)(udp_len + text_len));
    uint8_t cover[12 + 8 + 256];
    memcpy(cover, pseudo, sizeof pseudo);
    memcpy(cover + sizeof pseudo, udp, udp_len + text_len);
    uint16_t udp_sum = checksum16(cover, sizeof pseudo + udp_len + text_len);
    put_be16(udp + 6, udp_sum == 0 ? 0xffff : udp_sum);

    printf("%-10s %6s %6s\n", "layer", "offset", "length");
    printf("%-10s %6d %6lu\n", "Ethernet", 0, (unsigned long)eth_len);
    printf("%-10s %6lu %6lu\n", "IPv4", (unsigned long)eth_len,
           (unsigned long)ip_len);
    printf("%-10s %6lu %6lu\n", "UDP", (unsigned long)(eth_len + ip_len),
           (unsigned long)udp_len);
    printf("%-10s %6lu %6lu\n", "payload", (unsigned long)payload_off,
           (unsigned long)text_len);
    printf("%-10s %6s %6lu\n", "total", "", (unsigned long)total);
    printf("\n");

    for (size_t i = 0; i < total; i++) {
        printf("%02x%s", frame[i], (i % 16 == 15 || i + 1 == total) ? "\n" : " ");
    }

    /* 接收方验证：把校验和字段一起纳入求和，0xffff 表示通过 */
    memcpy(cover + sizeof pseudo, udp, udp_len + text_len);
    printf("\nreceiver check: 0x%04x\n",
           sum16(cover, sizeof pseudo + udp_len + text_len));
    return 0;
}