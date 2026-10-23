#include "frame.h"
#include "crc16.h"
#include <string.h>

void parser_init(struct frame_parser *p) {
    *p = (struct frame_parser){0};
    p->state = PARSE_WAIT_SYNC1;
}

enum parser_result parser_feed(struct frame_parser *p,
                               uint8_t byte,
                               struct frame *out_frame) {
    switch (p->state) {
    case PARSE_WAIT_SYNC1:
        if (byte == FRAME_SYNC1) {
            p->state = PARSE_WAIT_SYNC2;
        }
        return PARSE_INCOMPLETE;

    case PARSE_WAIT_SYNC2:
        if (byte == FRAME_SYNC2) {
            p->state = PARSE_LEN;
        } else if (byte == FRAME_SYNC1) {
            p->state = PARSE_WAIT_SYNC2;
        } else {
            p->state = PARSE_WAIT_SYNC1;
        }
        return PARSE_INCOMPLETE;

    case PARSE_LEN:
        if (byte > FRAME_MAX_PAYLOAD) {
            p->err_len_count++;
            p->state = PARSE_WAIT_SYNC1;
            return PARSE_ERR_LEN;
        }
        p->working.len = byte;
        p->crc_calc = crc16_ccitt_update(CRC16_CCITT_INIT, &byte, 1u);
        p->state = PARSE_CMD;
        return PARSE_INCOMPLETE;

    case PARSE_CMD:
        p->working.cmd = byte;
        p->crc_calc = crc16_ccitt_update(p->crc_calc, &byte, 1u);
        p->idx = 0u;
        if (p->working.len == 0u) {
            p->state = PARSE_CRC_LO;
        } else {
            p->state = PARSE_PAYLOAD;
        }
        return PARSE_INCOMPLETE;

    case PARSE_PAYLOAD:
        p->working.payload[p->idx++] = byte;
        p->crc_calc = crc16_ccitt_update(p->crc_calc, &byte, 1u);
        if (p->idx >= p->working.len) {
            p->state = PARSE_CRC_LO;
        }
        return PARSE_INCOMPLETE;

    case PARSE_CRC_LO:
        p->crc_rx = (uint16_t)byte;
        p->state = PARSE_CRC_HI;
        return PARSE_INCOMPLETE;

    case PARSE_CRC_HI:
        p->crc_rx |= (uint16_t)byte << 8;
        p->state = PARSE_WAIT_SYNC1;
        if (p->crc_rx == p->crc_calc) {
            p->ok_count++;
            *out_frame = p->working;
            return PARSE_OK;
        }
        p->err_crc_count++;
        return PARSE_ERR_CRC;

    default:
        p->state = PARSE_WAIT_SYNC1;
        return PARSE_INCOMPLETE;
    }
}

size_t frame_encode(uint8_t *out_buf,
                    size_t out_cap,
                    uint8_t cmd,
                    const uint8_t *payload,
                    uint8_t len) {
    if (out_buf == NULL || len > FRAME_MAX_PAYLOAD) {
        return 0u;
    }
    if (len > 0u && payload == NULL) {
        return 0u;
    }
    size_t total = (size_t)len + FRAME_OVERHEAD;
    if (out_cap < total) {
        return 0u;
    }

    out_buf[0] = FRAME_SYNC1;
    out_buf[1] = FRAME_SYNC2;
    out_buf[2] = len;
    out_buf[3] = cmd;
    if (len > 0u) {
        memcpy(&out_buf[4], payload, len);
    }

    uint16_t crc = crc16_ccitt(&out_buf[2], (size_t)len + 2u);
    out_buf[4u + len] = (uint8_t)(crc & 0xFFu);
    out_buf[5u + len] = (uint8_t)((crc >> 8) & 0xFFu);
    return total;
}

typedef void (*cmd_handler_fn)(struct device_state *dev,
                               const struct frame *req,
                               struct frame *resp);

static void handle_ping(struct device_state *dev,
                        const struct frame *req,
                        struct frame *resp) {
    (void)dev;
    resp->cmd = (uint8_t)(req->cmd | CMD_RESP_MASK);
    resp->len = req->len;
    if (req->len > 0u) {
        memcpy(resp->payload, req->payload, req->len);
    }
}

static void handle_get_status(struct device_state *dev,
                              const struct frame *req,
                              struct frame *resp) {
    if (req->len != 0u) {
        resp->cmd = CMD_ERROR;
        resp->len = 2u;
        resp->payload[0] = req->cmd;
        resp->payload[1] = ERR_BAD_PARAM;
        return;
    }
    resp->cmd = (uint8_t)(req->cmd | CMD_RESP_MASK);
    resp->len = 6u;
    /* 逐字节按小端编码，避免结构体对齐填充与字节序问题 */
    resp->payload[0] = (uint8_t)(dev->uptime_ms & 0xFFu);
    resp->payload[1] = (uint8_t)((dev->uptime_ms >> 8) & 0xFFu);
    resp->payload[2] = (uint8_t)((dev->uptime_ms >> 16) & 0xFFu);
    resp->payload[3] = (uint8_t)((dev->uptime_ms >> 24) & 0xFFu);
    resp->payload[4] = (uint8_t)(dev->rx_frames & 0xFFu);
    resp->payload[5] = (uint8_t)((dev->rx_frames >> 8) & 0xFFu);
}

static void handle_set_led(struct device_state *dev,
                           const struct frame *req,
                           struct frame *resp) {
    if (req->len != 1u || req->payload[0] > 1u) {
        resp->cmd = CMD_ERROR;
        resp->len = 2u;
        resp->payload[0] = req->cmd;
        resp->payload[1] = ERR_BAD_PARAM;
        return;
    }
    dev->led_on = req->payload[0];
    resp->cmd = (uint8_t)(req->cmd | CMD_RESP_MASK);
    resp->len = 1u;
    resp->payload[0] = dev->led_on;
}

struct cmd_entry {
    uint8_t cmd;
    cmd_handler_fn handler;
};

static const struct cmd_entry g_cmd_table[] = {
    { CMD_PING,       handle_ping       },
    { CMD_GET_STATUS, handle_get_status },
    { CMD_SET_LED,    handle_set_led    },
};

void cmd_dispatch(struct device_state *dev,
                  const struct frame *req,
                  struct frame *resp) {
    dev->rx_frames++;
    size_t n = sizeof(g_cmd_table) / sizeof(g_cmd_table[0]);
    for (size_t i = 0u; i < n; ++i) {
        if (g_cmd_table[i].cmd == req->cmd) {
            g_cmd_table[i].handler(dev, req, resp);
            return;
        }
    }

    resp->cmd = CMD_ERROR;
    resp->len = 2u;
    resp->payload[0] = req->cmd;
    resp->payload[1] = ERR_UNKNOWN_CMD;
}
