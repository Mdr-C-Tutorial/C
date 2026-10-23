#include "crc16.h"
#include "frame.h"
#include "ringbuf.h"
#include <stdio.h>
#include <string.h>

static void print_hex(const char *prefix, const uint8_t *buf, size_t len) {
    printf("%s (%u B):", prefix, (unsigned)len);
    for (size_t i = 0u; i < len; ++i) {
        printf(" %02X", (unsigned)buf[i]);
    }
    printf("\n");
}

static void feed_stream(struct ringbuf *rb,
                        struct frame_parser *parser,
                        struct device_state *dev,
                        const uint8_t *chunk,
                        size_t chunk_len) {
    for (size_t i = 0u; i < chunk_len; ++i) {
        (void)rb_put(rb, chunk[i]);
    }

    uint8_t byte = 0u;
    while (rb_get(rb, &byte)) {
        struct frame req;
        enum parser_result res = parser_feed(parser, byte, &req);
        if (res == PARSE_OK) {
            printf("  -> RX frame: cmd=0x%02X len=%u payload=",
                   (unsigned)req.cmd, (unsigned)req.len);
            for (uint8_t i = 0u; i < req.len; ++i) {
                printf("%02X ", (unsigned)req.payload[i]);
            }
            printf("\n");

            struct frame resp;
            cmd_dispatch(dev, &req, &resp);

            uint8_t wire[FRAME_MAX_WIRE];
            size_t n = frame_encode(wire, sizeof(wire),
                                    resp.cmd, resp.payload, resp.len);
            print_hex("  <- TX resp ", wire, n);
        } else if (res == PARSE_ERR_CRC) {
            printf("  -> RX error: CRC mismatch (err_crc=%u)\n",
                   (unsigned)parser->err_crc_count);
        } else if (res == PARSE_ERR_LEN) {
            printf("  -> RX error: invalid length (err_len=%u)\n",
                   (unsigned)parser->err_len_count);
        }
    }
}

int main(void) {
    const uint8_t std_vec[] = "123456789";
    uint16_t check = crc16_ccitt(std_vec, 9u);
    printf("CRC-16/CCITT-FALSE(\"123456789\") = 0x%04X\n\n", (unsigned)check);

    uint8_t rb_mem[128];
    struct ringbuf rb;
    rb_init(&rb, rb_mem, sizeof(rb_mem));

    struct frame_parser parser;
    parser_init(&parser);

    struct device_state dev = {
        .uptime_ms = 123456u,
        .rx_frames = 0u,
        .led_on = 0u,
    };

    /* 1. 单帧完整到达：PING "Hi" */
    printf("[Case 1] Single complete frame (PING \"Hi\")\n");
    uint8_t f1[FRAME_MAX_WIRE];
    const uint8_t ping_data[] = { 'H', 'i' };
    size_t f1_len = frame_encode(f1, sizeof(f1), CMD_PING, ping_data, 2u);
    print_hex("  raw wire", f1, f1_len);
    feed_stream(&rb, &parser, &dev, f1, f1_len);

    /* 2. 一帧拆成三段到达 */
    printf("\n[Case 2] One frame split into 3 chunks\n");
    uint8_t f2[FRAME_MAX_WIRE];
    const uint8_t echo_data[] = { 0x10u, 0x20u, 0x30u, 0x40u };
    size_t f2_len = frame_encode(f2, sizeof(f2), CMD_PING, echo_data, 4u);
    printf("  chunk 1 (3 B)...\n");
    feed_stream(&rb, &parser, &dev, &f2[0], 3u);
    printf("  chunk 2 (3 B)...\n");
    feed_stream(&rb, &parser, &dev, &f2[3], 3u);
    printf("  chunk 3 (%u B)...\n", (unsigned)(f2_len - 6u));
    feed_stream(&rb, &parser, &dev, &f2[6], f2_len - 6u);

    /* 3. 两帧粘连一次到达：SET_LED(1) + GET_STATUS() */
    printf("\n[Case 3] Two back-to-back frames in one read\n");
    uint8_t f3[FRAME_MAX_WIRE * 2u];
    const uint8_t led_arg = 1u;
    size_t n3a = frame_encode(f3, sizeof(f3), CMD_SET_LED, &led_arg, 1u);
    size_t n3b = frame_encode(&f3[n3a], sizeof(f3) - n3a,
                              CMD_GET_STATUS, NULL, 0u);
    feed_stream(&rb, &parser, &dev, f3, n3a + n3b);

    /* 4. 前置干扰字节（含伪造的 0xAA）+ 合法帧 */
    printf("\n[Case 4] Leading noise bytes (00 FF AA 12 AA) before valid frame\n");
    uint8_t f4[32];
    const uint8_t noise[] = { 0x00u, 0xFFu, 0xAAu, 0x12u, 0xAAu };
    memcpy(f4, noise, sizeof(noise));
    size_t n4 = frame_encode(&f4[sizeof(noise)],
                             sizeof(f4) - sizeof(noise),
                             CMD_GET_STATUS, NULL, 0u);
    feed_stream(&rb, &parser, &dev, f4, sizeof(noise) + n4);

    /* 5. CRC 损坏帧 + 未知命令帧恢复 */
    printf("\n[Case 5] Corrupted frame followed by unknown command 0x99\n");
    uint8_t f5[FRAME_MAX_WIRE * 2u];
    size_t n5a = frame_encode(f5, sizeof(f5), CMD_PING, ping_data, 2u);
    f5[4] ^= 0x01u; /* 翻转载荷中的 1 个位制造传输错误 */
    size_t n5b = frame_encode(&f5[n5a], sizeof(f5) - n5a, 0x99u, NULL, 0u);
    feed_stream(&rb, &parser, &dev, f5, n5a + n5b);

    printf("\nSummary: ok=%u, err_crc=%u, err_len=%u, led=%u\n",
           (unsigned)parser.ok_count, (unsigned)parser.err_crc_count,
           (unsigned)parser.err_len_count, (unsigned)dev.led_on);
    return 0;
}
