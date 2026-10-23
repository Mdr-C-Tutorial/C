#include "../crc16.h"
#include "../frame.h"
#include <stdio.h>

#define PARSER_TIMEOUT_MS 50u

struct timeout_parser {
    struct frame_parser base;
    uint32_t last_rx_ms;
    uint32_t timeout_count;
};

void timeout_parser_init(struct timeout_parser *tp) {
    parser_init(&tp->base);
    tp->last_rx_ms = 0u;
    tp->timeout_count = 0u;
}

/*
 * TODO：实现带半帧超时恢复的字节喂入函数。
 * 要求：
 * 1. 若 tp->base.state != PARSE_WAIT_SYNC1，且
 *    (uint32_t)(now_ms - tp->last_rx_ms) > PARSER_TIMEOUT_MS，
 *    将 tp->base.state 复位为 PARSE_WAIT_SYNC1，并令 tp->timeout_count++。
 * 2. 更新 tp->last_rx_ms = now_ms，再调用 parser_feed(&tp->base, byte, out_frame)。
 */
enum parser_result timeout_parser_feed(struct timeout_parser *tp,
                                       uint8_t byte,
                                       uint32_t now_ms,
                                       struct frame *out_frame) {
    (void)tp;
    (void)byte;
    (void)now_ms;
    (void)out_frame;
    return PARSE_INCOMPLETE;
}

int main(void) {
    struct timeout_parser tp;
    timeout_parser_init(&tp);

    /* 在 t = 10..12 ms 喂入半截帧：AA 55 04（声明载荷 4 字节后断线） */
    const uint8_t half_frame[] = { 0xAAu, 0x55u, 0x04u };
    struct frame out = {0};
    for (size_t i = 0u; i < sizeof(half_frame); ++i) {
        (void)timeout_parser_feed(&tp, half_frame[i], 10u + (uint32_t)i, &out);
    }

    /* 在 t = 100 ms 发送一帧完整的 GET_STATUS 帧 */
    uint8_t valid_frame[FRAME_MAX_WIRE];
    size_t n = frame_encode(valid_frame, sizeof(valid_frame),
                            CMD_GET_STATUS, NULL, 0u);
    enum parser_result res = PARSE_INCOMPLETE;
    for (size_t i = 0u; i < n; ++i) {
        res = timeout_parser_feed(&tp, valid_frame[i], 100u + (uint32_t)i, &out);
    }

    printf("res=%d (expected %d), cmd=0x%02X, timeout_count=%u (expected 1)\n",
           (int)res, (int)PARSE_OK, (unsigned)out.cmd, (unsigned)tp.timeout_count);
    return 0;
}
