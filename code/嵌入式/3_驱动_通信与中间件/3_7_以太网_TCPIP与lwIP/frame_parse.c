#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* 从网络字节序（大端）读取 16 位无符号整数 */
static uint16_t read_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

/* RFC 1071：计算 16 位反码和的二进制反码（互联网校验和） */
uint16_t inet_checksum(const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t sum = 0u;

    while (len > 1u) {
        sum += read_be16(bytes);
        bytes += 2;
        len -= 2u;
    }
    if (len == 1u) {
        sum += (uint32_t)bytes[0] << 8;
    }

    while ((sum >> 16) != 0u) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (uint16_t)(~sum & 0xFFFFu);
}

static void print_mac(const char *label, const uint8_t mac[6]) {
    printf("%s: %02x:%02x:%02x:%02x:%02x:%02x\n",
           label, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void print_ipv4(const char *label, const uint8_t ip[4]) {
    printf("%s: %u.%u.%u.%u\n", label, ip[0], ip[1], ip[2], ip[3]);
}

static void parse_and_verify_frame(const uint8_t *frame, size_t len) {
    if (len < 14u + 20u) {
        printf("Frame too short (%u bytes)\n", (unsigned)len);
        return;
    }

    /* 1. 解析 14 字节以太网首部 */
    const uint8_t *dst_mac = &frame[0];
    const uint8_t *src_mac = &frame[6];
    uint16_t eth_type = read_be16(&frame[12]);

    print_mac("Dst MAC", dst_mac);
    print_mac("Src MAC", src_mac);
    printf("EtherType: 0x%04X (%s)\n",
           eth_type, eth_type == 0x0800u ? "IPv4" : "Other");

    if (eth_type != 0x0800u) {
        return;
    }

    /* 2. 解析 IPv4 首部 */
    const uint8_t *ip = &frame[14];
    uint8_t version = (uint8_t)(ip[0] >> 4);
    uint8_t ihl = (uint8_t)(ip[0] & 0x0Fu);
    size_t ip_hdr_len = (size_t)ihl * 4u;
    uint16_t total_len = read_be16(&ip[2]);
    uint8_t ttl = ip[8];
    uint8_t proto = ip[9];
    uint16_t rx_hdr_sum = read_be16(&ip[10]);

    printf("IPv4: version=%u, hdr_len=%u, total_len=%u, ttl=%u, proto=%u\n",
           version, (unsigned)ip_hdr_len, total_len, ttl, proto);
    print_ipv4("Src IP", &ip[12]);
    print_ipv4("Dst IP", &ip[16]);

    uint16_t verify_ip = inet_checksum(ip, ip_hdr_len);
    printf("IPv4 header checksum=0x%04X, verify_result=0x%04X [%s]\n",
           rx_hdr_sum, verify_ip, verify_ip == 0u ? "OK" : "BAD");

    /* 3. 解析 ICMP 回显请求（proto == 1） */
    if (proto == 1u && len >= 14u + ip_hdr_len + 8u) {
        const uint8_t *icmp = ip + ip_hdr_len;
        size_t icmp_len = (size_t)total_len - ip_hdr_len;
        uint8_t type = icmp[0];
        uint8_t code = icmp[1];
        uint16_t icmp_id = read_be16(&icmp[4]);
        uint16_t icmp_seq = read_be16(&icmp[6]);
        uint16_t verify_icmp = inet_checksum(icmp, icmp_len);

        printf("ICMP: type=%u (Echo Request), code=%u, id=0x%04X, seq=%u, "
               "verify_result=0x%04X [%s]\n",
               type, code, icmp_id, icmp_seq, verify_icmp,
               verify_icmp == 0u ? "OK" : "BAD");
    }
}

int main(void) {
    /* 一帧完整的 50 字节以太网帧：14B Eth + 20B IPv4 + 16B ICMP Echo Request */
    uint8_t frame[50] = {
        /* Ethernet II: dst=02:00:00:12:34:56, src=aa:bb:cc:dd:ee:ff, type=0x0800 */
        0x02, 0x00, 0x00, 0x12, 0x34, 0x56,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
        0x08, 0x00,
        /* IPv4: ver/IHL=0x45, TOS=0, len=36 (0x0024), id=0x1234, flags=0x4000 (DF),
         *       TTL=64 (0x40), proto=1 (ICMP), chksum=0x0000 (稍后填入),
         *       src=192.168.1.100, dst=192.168.1.50 */
        0x45, 0x00, 0x00, 0x24, 0x12, 0x34, 0x40, 0x00,
        0x40, 0x01, 0x00, 0x00,
        192, 168, 1, 100,
        192, 168, 1, 50,
        /* ICMP Echo Request: type=8, code=0, chksum=0x0000, id=0x0001, seq=1,
         *                    payload="lwip-eth" */
        0x08, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01,
        'l', 'w', 'i', 'p', '-', 'e', 't', 'h'
    };

    /* 计算并填入 IPv4 首部校验和与 ICMP 校验和 */
    uint16_t ip_sum = inet_checksum(&frame[14], 20u);
    frame[24] = (uint8_t)(ip_sum >> 8);
    frame[25] = (uint8_t)(ip_sum & 0xFFu);

    uint16_t icmp_sum = inet_checksum(&frame[34], 16u);
    frame[36] = (uint8_t)(icmp_sum >> 8);
    frame[37] = (uint8_t)(icmp_sum & 0xFFu);

    printf("--- Verify valid frame ---\n");
    parse_and_verify_frame(frame, sizeof(frame));

    printf("\n--- Flip 1 bit in TTL and re-verify ---\n");
    frame[22] ^= 0x01u;
    uint16_t corrupted_check = inet_checksum(&frame[14], 20u);
    printf("After 1-bit flip: verify_result=0x%04X [%s]\n",
           corrupted_check, corrupted_check == 0u ? "OK" : "BAD");

    return 0;
}
