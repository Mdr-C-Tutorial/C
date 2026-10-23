/*
 * 生成第 9 章习题用的数据文件。用法：make_data DIR
 *
 * 输出：
 *   udp_frame.txt    一个 UDP over IPv4 over Ethernet 帧
 *   tcp_frame.txt    一个 TCP over IPv4 over Ethernet 帧
 *   frames_mixed.txt 四个帧，其中两个的校验和被破坏
 *   frames_many.txt  100 个帧，含 ARP、UDP、TCP
 *   segments.txt     一条按行协议字节流被切成的若干段
 *   tcp_stream.txt   一条 TCP 连接的段，按到达顺序记录
 *   tcp_log.txt      一条连接上 SEND 与 ACK 事件
 *   payload.txt      40907 用的输入文本
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

static void put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void pseudo_header(uint8_t *p, const uint8_t *ip, uint8_t protocol,
                          uint16_t len) {
    memcpy(p, ip + 12, 8);
    p[8] = 0;
    p[9] = protocol;
    put16(p + 10, len);
}

static size_t ipv4_header(uint8_t *out, uint8_t protocol,
                          const uint8_t *payload, size_t plen, uint16_t id) {
    uint16_t total = (uint16_t)(20 + plen);

    memset(out, 0, 20);
    out[0] = 0x45;
    put16(out + 2, total);
    put16(out + 4, id);
    put16(out + 6, 0x4000);
    out[8] = 64;
    out[9] = protocol;
    out[12] = 192;
    out[13] = 168;
    out[14] = 1;
    out[15] = 10;
    out[16] = 192;
    out[17] = 168;
    out[18] = 1;
    out[19] = 20;
    put16(out + 10, checksum16(out, 20));
    memcpy(out + 20, payload, plen);
    return 20 + plen;
}

static size_t ethernet_header(uint8_t *frame, uint16_t type) {
    memcpy(frame, "\x02\x00\x00\x00\x00\x01", 6);
    memcpy(frame + 6, "\x02\x00\x00\x00\x00\x02", 6);
    put16(frame + 12, type);
    return 14;
}

static size_t udp_frame(uint8_t *frame, uint16_t sport, uint16_t dport,
                        const uint8_t *data, size_t dlen, uint16_t id) {
    uint8_t seg[8 + 256];
    uint8_t pseudo[12 + 8 + 256];
    uint16_t ulen = (uint16_t)(8 + dlen);
    size_t off = ethernet_header(frame, 0x0800);

    put16(seg, sport);
    put16(seg + 2, dport);
    put16(seg + 4, ulen);
    put16(seg + 6, 0);
    memcpy(seg + 8, data, dlen);
    ipv4_header(frame + off, 17, seg, ulen, id);
    pseudo_header(pseudo, frame + off, 17, ulen);
    memcpy(pseudo + 12, seg, ulen);
    uint16_t c = checksum16(pseudo, 12 + ulen);
    put16(seg + 6, c == 0 ? 0xffff : c);
    memcpy(frame + off + 20, seg, ulen);
    return off + 20 + ulen;
}

static size_t tcp_frame(uint8_t *frame, uint16_t sport, uint16_t dport,
                        uint32_t seq, uint32_t ack, uint8_t flags,
                        const uint8_t *data, size_t dlen, uint16_t id) {
    uint8_t seg[20 + 256];
    uint8_t pseudo[12 + 20 + 256];
    size_t off = ethernet_header(frame, 0x0800);

    memset(seg, 0, 20);
    put16(seg, sport);
    put16(seg + 2, dport);
    put32(seg + 4, seq);
    put32(seg + 8, ack);
    seg[12] = 0x50;
    seg[13] = flags;
    put16(seg + 14, 64240);
    memcpy(seg + 20, data, dlen);
    uint16_t tlen = (uint16_t)(20 + dlen);
    ipv4_header(frame + off, 6, seg, tlen, id);
    pseudo_header(pseudo, frame + off, 6, tlen);
    memcpy(pseudo + 12, seg, tlen);
    put16(seg + 16, checksum16(pseudo, 12 + tlen));
    memcpy(frame + off + 20, seg, tlen);
    return off + 20 + tlen;
}

static size_t arp_frame(uint8_t *frame, int k) {
    memset(frame, 0, 42);
    memset(frame, 0xff, 6);
    memcpy(frame + 6, "\x02\x00\x00\x00\x00\x02", 6);
    put16(frame + 12, 0x0806);
    put16(frame + 14, 1);
    put16(frame + 16, 0x0800);
    frame[18] = 6;
    frame[19] = 4;
    put16(frame + 20, 1);
    frame[22] = 0x02;
    frame[26] = 192;
    frame[27] = 168;
    frame[28] = 1;
    frame[29] = (uint8_t)(10 + k);
    memset(frame + 32, 0, 6);
    frame[38] = 192;
    frame[39] = 168;
    frame[40] = 1;
    frame[41] = (uint8_t)(10 + k);
    return 42;
}

static void write_hex_frame(FILE *f, const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; i++) {
        fprintf(f, "%02x%s", p[i], (i % 16 == 15 || i + 1 == n) ? "\n" : " ");
    }
    fprintf(f, "\n");
}

static FILE *open_in(const char *dir, const char *name) {
    char path[512];
    FILE *f;

    snprintf(path, sizeof path, "%s/%s", dir, name);
    f = fopen(path, "w");
    if (f == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    return f;
}

static int payload_corruptions = 0;
static int address_corruptions = 0;

static void many_frames(const char *dir) {
    static const uint16_t udp_ports[3][2] = {
        {40000, 21000}, {40001, 21001}, {40002, 21000}};
    static const uint16_t tcp_ports[2][2] = {{52100, 21000}, {52101, 21001}};
    uint8_t frame[512];
    uint8_t data[256];
    FILE *f = open_in(dir, "frames_many.txt");
    uint16_t id = 0x2000;

    for (int i = 0; i < 100; i++) {
        if (i >= 90) {
            write_hex_frame(f, frame, arp_frame(frame, i - 90));
            continue;
        }
        size_t dlen;
        int is_udp = i % 5 < 3;
        size_t n;
        if (is_udp) {
            int k = (i / 5) % 3;
            dlen = (size_t)(10 + (i * 7) % 50);
            for (size_t j = 0; j < dlen; j++) {
                data[j] = (uint8_t)('a' + (int)((j + (size_t)i) % 26));
            }
            n = udp_frame(frame, udp_ports[k][0], udp_ports[k][1], data, dlen,
                          id++);
        } else {
            int k = (i / 5) % 2;
            dlen = (size_t)(20 + (i * 11) % 60);
            memset(data, 'A' + (i % 26), dlen);
            n = tcp_frame(frame, tcp_ports[k][0], tcp_ports[k][1],
                          (uint32_t)(0x10000000u + (uint32_t)i * 1000u),
                          0x20000000u, 0x18, data, dlen, id++);
        }
        if (i % 17 == 3) {
            frame[n - 2] ^= 0x20;
            payload_corruptions++;
        }
        if (i % 23 == 7) {
            frame[14 + 15] ^= 0x01;
            address_corruptions++;
        }
        write_hex_frame(f, frame, n);
    }
    fclose(f);
}

static void segments_file(const char *dir) {
    static const char *messages[] = {
        "GET /index.html\n", "GET /style.css\n", "GET /app.js\n",
        "HELLO world\n",     "QUIT\n",           "PARTIAL LINE",
    };
    static const size_t cuts[] = {7, 13, 5, 11, 3, 17, 9, 4, 6, 12};
    uint8_t stream[512];
    size_t len = 0;
    FILE *f;

    for (size_t i = 0; i < sizeof messages / sizeof messages[0]; i++) {
        size_t n = strlen(messages[i]);
        memcpy(stream + len, messages[i], n);
        len += n;
    }
    f = open_in(dir, "segments.txt");
    for (size_t pos = 0, k = 0; pos < len; k++) {
        size_t take = cuts[k % (sizeof cuts / sizeof cuts[0])];
        if (take > len - pos) {
            take = len - pos;
        }
        for (size_t j = 0; j < take; j++) {
            fprintf(f, "%02x", stream[pos + j]);
        }
        fprintf(f, "\n");
        pos += take;
    }
    fclose(f);
}

static void tcp_stream_file(const char *dir) {
    static const char *lines[] = {
        "GET /alpha HTTP/1.0\r\n",  "GET /bravo HTTP/1.0\r\n",
        "GET /charlie HTTP/1.0\r\n", "GET /delta HTTP/1.0\r\n",
        "GET /echo HTTP/1.0\r\n",   "GET /foxtrot HTTP/1.0\r\n",
    };
    static const int arrival[] = {0, 3, 1, 5, 2, 2, 4, 8, 7};
    uint8_t stream[512];
    size_t starts[64];
    size_t lens[64];
    size_t len = 0;
    int count = 0;
    FILE *f;

    for (size_t i = 0; i < sizeof lines / sizeof lines[0]; i++) {
        size_t n = strlen(lines[i]);
        memcpy(stream + len, lines[i], n);
        len += n;
    }
    for (size_t at = 0; at < len; at += 12) {
        starts[count] = at;
        lens[count] = len - at < 12 ? len - at : 12;
        count++;
    }
    f = open_in(dir, "tcp_stream.txt");
    fprintf(f, "# 192.168.1.10:52100 -> 192.168.1.20:21000, 初始序列号 5000\n");
    fprintf(f, "# 每行一段，按到达顺序记录：seq 是段内第一个字节的序列号\n");
    for (size_t i = 0; i < sizeof arrival / sizeof arrival[0]; i++) {
        int k = arrival[i];
        if (k >= count) {
            continue;
        }
        fprintf(f, "seq=%lu len=%lu hex=", (unsigned long)(5000 + starts[k]),
                (unsigned long)lens[k]);
        for (size_t j = 0; j < lens[k]; j++) {
            fprintf(f, "%02x", stream[starts[k] + j]);
        }
        fprintf(f, "\n");
    }
    fclose(f);
}

static void tcp_log_file(const char *dir) {
    static const char *events[] = {
        "SEND seq=1000 len=200", "SEND seq=1200 len=300", "ACK ack=1500",
        "SEND seq=1500 len=500", "ACK ack=2000",          "SEND seq=2000 len=300",
        "SEND seq=2300 len=200", "ACK ack=2300",          "SEND seq=2300 len=200",
        "ACK ack=2500",          "SEND seq=2500 len=100", "ACK ack=2600",
    };
    FILE *f = open_in(dir, "tcp_log.txt");

    fprintf(f, "# 客户端发送与服务器确认的事件，按发生顺序\n");
    for (size_t i = 0; i < sizeof events / sizeof events[0]; i++) {
        fprintf(f, "%s\n", events[i]);
    }
    fclose(f);
}

static void payload_file(const char *dir) {
    static const char *lines[] = {
        "boot ok",
        "disk /dev/sda1 mounted",
        "network 192.168.1.10 up",
        "user alice logged in from 192.168.1.77",
        "job 42 finished with status 0 after 1200 ms",
        "warning: retrying connection to 192.168.1.20:21000 (attempt 3 of 5)",
        "error: checksum mismatch on block 88123 of device /dev/sdb",
        "shutdown complete",
    };
    FILE *f = open_in(dir, "payload.txt");

    for (int repeat = 0; repeat < 64; repeat++) {
        for (size_t i = 0; i < sizeof lines / sizeof lines[0]; i++) {
            fprintf(f, "%s\n", lines[i]);
        }
    }
    fclose(f);
}

int main(int argc, char **argv) {
    const char *dir;
    uint8_t frame[512];
    uint8_t data[256];
    size_t n;
    FILE *f;

    if (argc != 2) {
        fprintf(stderr, "usage: %s DIR\n", argv[0]);
        return 2;
    }
    dir = argv[1];

    f = open_in(dir, "udp_frame.txt");
    n = udp_frame(frame, 40000, 21000, (const uint8_t *)"hello udp\n", 10, 0x1c46);
    write_hex_frame(f, frame, n);
    fclose(f);

    f = open_in(dir, "tcp_frame.txt");
    n = tcp_frame(frame, 52100, 21000, 439041101u, 889901u, 0x18,
                  (const uint8_t *)"GET / HTTP/1.0\r\n\r\n", 18, 0x1c47);
    write_hex_frame(f, frame, n);
    fclose(f);

    uint8_t good_udp[512];
    uint8_t good_tcp[512];
    uint8_t bad_udp[512];
    uint8_t bad_ip[512];
    size_t nu = udp_frame(good_udp, 40000, 21000, (const uint8_t *)"hello udp\n",
                          10, 0x1c46);
    size_t nt = tcp_frame(good_tcp, 52100, 21000, 439041101u, 889901u, 0x18,
                          (const uint8_t *)"GET / HTTP/1.0\r\n\r\n", 18, 0x1c47);
    memcpy(bad_udp, good_udp, nu);
    bad_udp[14 + 20 + 8 + 4] = 'O';
    memcpy(bad_ip, good_tcp, nt);
    bad_ip[14 + 15] = 0x0b;

    f = open_in(dir, "frames_mixed.txt");
    write_hex_frame(f, good_udp, nu);
    write_hex_frame(f, good_tcp, nt);
    write_hex_frame(f, bad_udp, nu);
    write_hex_frame(f, bad_ip, nt);
    fclose(f);

    many_frames(dir);
    segments_file(dir);
    tcp_stream_file(dir);
    tcp_log_file(dir);
    payload_file(dir);

    memset(data, 0, sizeof data);
    (void)data;
    printf("%s: payload corruptions %d, address corruptions %d\n", dir,
           payload_corruptions, address_corruptions);
    return 0;
}