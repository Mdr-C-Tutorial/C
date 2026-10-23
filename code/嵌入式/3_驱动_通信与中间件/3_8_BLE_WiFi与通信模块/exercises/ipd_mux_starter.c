/*
 * 习题 60381 初始代码：支持单连接 +IPD,<len>:<data> 与多连接 +IPD,<id>,<len>:<data>。
 *
 * 编译：
 *   gcc -std=c17 -Wall -Wextra exercises/ipd_mux_starter.c -o ipd_mux_starter
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define IPD_MAX 256u

enum rx_state {
    RX_LINE = 0,
    RX_IPD_HEADER = 1,
    RX_IPD_DATA = 2,
};

typedef void (*ipd_mux_cb_t)(int link_id, const uint8_t *data, size_t len);

struct ipd_parser {
    enum rx_state state;
    char line[128];
    size_t line_len;

    int link_id;
    size_t accum;
    size_t expected;
    size_t received;
    bool has_comma;
    uint8_t payload[IPD_MAX];
    ipd_mux_cb_t cb;
};

static void on_ipd_packet(int link_id, const uint8_t *data, size_t len) {
    printf("link=%d len=%u data=\"", link_id, (unsigned)len);
    for (size_t i = 0u; i < len; ++i) {
        putchar((int)data[i]);
    }
    printf("\"\n");
}

/*
 * TODO：补全 ipd_feed_byte() 中 RX_IPD_HEADER 分支的解析逻辑：
 *   - 若未遇到逗号即遇到 ':'，说明是单连接格式 +IPD,<len>:，link_id 记为 -1，
 *     expected = accum；
 *   - 若先遇到 ','，把當前 accum 存入 link_id（0～4），清零 accum 继续读长度；
 *   - 再遇到 ':' 时，expected = accum，进入 RX_IPD_DATA 定长接收。
 */
static void ipd_feed_byte(struct ipd_parser *p, uint8_t b) {
    if (p->state == RX_IPD_HEADER) {
        if (b >= (uint8_t)'0' && b <= (uint8_t)'9') {
            p->accum = p->accum * 10u + (size_t)(b - (uint8_t)'0');
        } else if (b == (uint8_t)',' && !p->has_comma) {
            p->link_id = (int)p->accum;
            p->accum = 0u;
            p->has_comma = true;
        } else if (b == (uint8_t)':') {
            p->expected = p->accum;
            if (p->expected > 0u && p->expected <= IPD_MAX) {
                p->received = 0u;
                p->state = RX_IPD_DATA;
            } else {
                p->state = RX_LINE;
                p->line_len = 0u;
            }
        } else {
            p->state = RX_LINE;
            p->line_len = 0u;
        }
        return;
    }

    if (p->state == RX_IPD_DATA) {
        p->payload[p->received++] = b;
        if (p->received == p->expected) {
            p->state = RX_LINE;
            p->line_len = 0u;
            if (p->cb != NULL) {
                p->cb(p->link_id, p->payload, p->expected);
            }
        }
        return;
    }

    if (b == (uint8_t)'\r' || b == (uint8_t)'\n') {
        p->line_len = 0u;
        return;
    }
    if (p->line_len < sizeof(p->line) - 1u) {
        p->line[p->line_len++] = (char)b;
    }
    if (p->line_len == 5u && memcmp(p->line, "+IPD,", 5u) == 0) {
        p->line_len = 0u;
        p->link_id = -1;
        p->accum = 0u;
        p->has_comma = false;
        p->state = RX_IPD_HEADER;
    }
}

int main(void) {
    struct ipd_parser parser = {0};
    parser.cb = on_ipd_packet;

    const char stream[] =
        "\r\n+IPD,5:world\r\n"
        "+IPD,0,4:ping\r\n"
        "+IPD,2,6:sensor\r\n";

    for (size_t i = 0u; i < sizeof(stream) - 1u; ++i) {
        ipd_feed_byte(&parser, (uint8_t)stream[i]);
    }
    return 0;
}
