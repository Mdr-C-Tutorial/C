/*
 * cdc_frame_starter.c：习题 63351 起始代码。
 * 在 USB CDC 虚拟串口之上接入 3.4 节的数据帧解析器与命令分发器。
 *
 * 主机自测编译运行：
 *   gcc -std=c17 -Wall -Wextra -I../../3_4_数据帧_环形缓冲区与状态机 \
 *       cdc_frame_starter.c \
 *       ../../3_4_数据帧_环形缓冲区与状态机/frame.c \
 *       ../../3_4_数据帧_环形缓冲区与状态机/crc16.c \
 *       -o cdc_frame_test
 *   ./cdc_frame_test
 */
#include "frame.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 主机测试桩：模拟 TinyUSB 的 tud_cdc_* 收发队列 */
static uint8_t g_mock_rx_buf[128];
static size_t g_mock_rx_pos = 0u;
static size_t g_mock_rx_len = 0u;

static uint8_t g_mock_tx_buf[128];
static size_t g_mock_tx_len = 0u;

static uint32_t tud_cdc_available(void) {
    return (uint32_t)(g_mock_rx_len - g_mock_rx_pos);
}

static uint32_t tud_cdc_read(void *buffer, uint32_t bufsize) {
    uint32_t avail = tud_cdc_available();
    uint32_t n = (bufsize < avail) ? bufsize : avail;
    if (n > 0u) {
        memcpy(buffer, &g_mock_rx_buf[g_mock_rx_pos], n);
        g_mock_rx_pos += n;
    }
    return n;
}

static uint32_t tud_cdc_write(const void *buffer, uint32_t bufsize) {
    size_t space = sizeof g_mock_tx_buf - g_mock_tx_len;
    uint32_t n = (bufsize < space) ? bufsize : (uint32_t)space;
    if (n > 0u) {
        memcpy(&g_mock_tx_buf[g_mock_tx_len], buffer, n);
        g_mock_tx_len += n;
    }
    return n;
}

static uint32_t tud_cdc_write_flush(void) {
    return (uint32_t)g_mock_tx_len;
}

/*
 * 从 USB CDC 接收缓冲区取字节送入帧解析器，每收齐一帧就调用 cmd_dispatch()
 * 并将编码后的应答帧经 tud_cdc_write() + tud_cdc_write_flush() 发回主机。
 */
void cdc_frame_task(struct frame_parser *parser, struct device_state *dev) {
    while (tud_cdc_available() > 0u) {
        uint8_t rx_chunk[64];
        uint32_t count = tud_cdc_read(rx_chunk, sizeof rx_chunk);
        for (uint32_t i = 0u; i < count; ++i) {
            struct frame req;
            if (parser_feed(parser, rx_chunk[i], &req) == PARSE_OK) {
                struct frame resp;
                cmd_dispatch(dev, &req, &resp);

                uint8_t wire[FRAME_MAX_WIRE];
                size_t wire_len = frame_encode(wire, sizeof wire,
                                               resp.cmd, resp.payload, resp.len);
                if (wire_len > 0u) {
                    tud_cdc_write(wire, (uint32_t)wire_len);
                    tud_cdc_write_flush();
                }
            }
        }
    }
}

int main(void) {
    struct frame_parser parser;
    parser_init(&parser);
    struct device_state dev = { .uptime_ms = 12000u, .rx_frames = 0u, .led_on = 0u };

    /* 构造两帧请求：CMD_SET_LED(1) 与 CMD_GET_STATUS()，连续放入模拟接收流 */
    uint8_t led_arg = 1u;
    size_t n1 = frame_encode(g_mock_rx_buf, sizeof g_mock_rx_buf,
                             CMD_SET_LED, &led_arg, 1u);
    size_t n2 = frame_encode(&g_mock_rx_buf[n1], sizeof g_mock_rx_buf - n1,
                             CMD_GET_STATUS, NULL, 0u);
    g_mock_rx_len = n1 + n2;

    cdc_frame_task(&parser, &dev);

    printf("led_on=%u, rx_frames=%u, tx_bytes=%u:",
           dev.led_on, dev.rx_frames, (unsigned)g_mock_tx_len);
    for (size_t i = 0u; i < g_mock_tx_len; ++i) {
        printf(" %02X", g_mock_tx_buf[i]);
    }
    printf("\n");
    return 0;
}
