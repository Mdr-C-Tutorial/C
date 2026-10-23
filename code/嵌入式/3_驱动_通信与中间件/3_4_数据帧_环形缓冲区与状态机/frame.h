#ifndef FRAME_H
#define FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FRAME_SYNC1       0xAAu
#define FRAME_SYNC2       0x55u
#define FRAME_MAX_PAYLOAD 64u
#define FRAME_OVERHEAD    6u
#define FRAME_MAX_WIRE    (FRAME_MAX_PAYLOAD + FRAME_OVERHEAD)

enum frame_cmd {
    CMD_PING       = 0x01u,
    CMD_GET_STATUS = 0x02u,
    CMD_SET_LED    = 0x03u,
    CMD_RESP_MASK  = 0x80u,
    CMD_ERROR      = 0xFFu,
};

enum frame_err_code {
    ERR_UNKNOWN_CMD = 0x01u,
    ERR_BAD_PARAM   = 0x02u,
};

struct frame {
    uint8_t cmd;
    uint8_t len;
    uint8_t payload[FRAME_MAX_PAYLOAD];
};

enum parser_state {
    PARSE_WAIT_SYNC1 = 0,
    PARSE_WAIT_SYNC2,
    PARSE_LEN,
    PARSE_CMD,
    PARSE_PAYLOAD,
    PARSE_CRC_LO,
    PARSE_CRC_HI,
};

enum parser_result {
    PARSE_INCOMPLETE = 0,
    PARSE_OK         = 1,
    PARSE_ERR_LEN    = -1,
    PARSE_ERR_CRC    = -2,
};

struct frame_parser {
    enum parser_state state;
    struct frame working;
    uint8_t idx;
    uint16_t crc_calc;
    uint16_t crc_rx;
    uint32_t ok_count;
    uint32_t err_len_count;
    uint32_t err_crc_count;
};

void parser_init(struct frame_parser *p);

enum parser_result parser_feed(struct frame_parser *p,
                               uint8_t byte,
                               struct frame *out_frame);

size_t frame_encode(uint8_t *out_buf,
                    size_t out_cap,
                    uint8_t cmd,
                    const uint8_t *payload,
                    uint8_t len);

struct device_state {
    uint32_t uptime_ms;
    uint16_t rx_frames;
    uint8_t led_on;
};

void cmd_dispatch(struct device_state *dev,
                  const struct frame *req,
                  struct frame *resp);

#endif
