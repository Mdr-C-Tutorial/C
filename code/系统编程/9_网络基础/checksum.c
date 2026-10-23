/*
 * 互联网校验和（16 位反码和），正文 2.3 节的示例。
 *
 *   gcc -std=c17 -Wall -Wextra -g checksum.c -o checksum
 *   ./checksum
 */
#include <stdint.h>
#include <stdio.h>

/* 16 位反码和：相邻两字节按大端拼成一个 16 位数相加，进位加回最低位 */
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

int main(void) {
    static const uint8_t example[] = {0x00, 0x01, 0xf2, 0x03,
                                      0xf4, 0xf5, 0xf6, 0xf7};
    /* 习题数据里那个 UDP 帧的 IPv4 首部，校验和字段先置 0 */
    static const uint8_t ip_header[20] = {
        0x45, 0x00, 0x00, 0x26, 0x1c, 0x46, 0x40, 0x00, 0x40, 0x11,
        0x00, 0x00, 0xc0, 0xa8, 0x01, 0x0a, 0xc0, 0xa8, 0x01, 0x14,
    };
    uint8_t header[20];

    printf("RFC 1071 example: checksum = 0x%04x\n",
           checksum16(example, sizeof example));

    for (size_t i = 0; i < sizeof header; i++) {
        header[i] = ip_header[i];
    }
    uint16_t c = checksum16(header, sizeof header);
    printf("IPv4 header checksum = 0x%04x\n", c);
    header[10] = (uint8_t)(c >> 8);
    header[11] = (uint8_t)(c & 0xff);
    printf("sum over the header including the checksum = 0x%04x\n",
           sum16(header, sizeof header));
    return 0;
}