#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    uint16_t host_port = 21000;
    uint32_t host_addr = 0x7f000001u;

    uint16_t net_port = htons(host_port);
    uint32_t net_addr = htonl(host_addr);

    printf("host order: port=0x%04x addr=0x%08x\n", host_port, host_addr);
    printf("network order: port=0x%04x addr=0x%08x\n", net_port, net_addr);
    printf("back again: port=%u addr=0x%08x\n", ntohs(net_port),
           ntohl(net_addr));

    uint32_t be = htonl(host_addr);
    const unsigned char *raw = (const unsigned char *)&be;
    printf("127.0.0.1 on the wire: %u.%u.%u.%u\n", raw[0],
           raw[1], raw[2], raw[3]);

    struct in_addr a;
    if (inet_pton(AF_INET, "192.168.1.10", &a) != 1) {
        return 1;
    }
    char text[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &a, text, sizeof text) == NULL) {
        return 1;
    }
    printf("inet_pton then inet_ntop: %s\n", text);

    struct in6_addr a6;
    if (inet_pton(AF_INET6, "::1", &a6) != 1) {
        return 1;
    }
    char text6[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, &a6, text6, sizeof text6) == NULL) {
        return 1;
    }
    printf("IPv6 loopback: %s (buffer %d bytes)\n", text6, INET6_ADDRSTRLEN);
    return 0;
}