#ifndef AT_PARSER_H
#define AT_PARSER_H

#include "ringbuf.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AT_LINE_MAX 128u
#define AT_IPD_MAX  256u

enum at_result {
    AT_RES_OK = 0,
    AT_RES_ERROR = -1,
    AT_RES_BUSY = -2,
    AT_RES_TIMEOUT = -3,
};

enum at_rx_state {
    AT_RX_LINE = 0,
    AT_RX_IPD_LEN = 1,
    AT_RX_IPD_DATA = 2,
};

typedef void (*at_write_fn_t)(const uint8_t *data, size_t len, void *user_data);
typedef void (*at_cmd_cb_t)(enum at_result result,
                            const char *info,
                            void *user_data);
typedef void (*at_urc_cb_t)(const char *urc_line, void *user_data);
typedef void (*at_ipd_cb_t)(const uint8_t *data, size_t len, void *user_data);
typedef void (*at_prompt_cb_t)(void *user_data);

struct at_client {
    struct ringbuf *rx_rb;
    at_write_fn_t write_fn;
    void *io_user;

    enum at_rx_state state;
    char line_buf[AT_LINE_MAX];
    size_t line_len;

    uint8_t ipd_buf[AT_IPD_MAX];
    size_t ipd_expected;
    size_t ipd_received;

    bool pending;
    bool expect_prompt;
    bool prompt_seen;
    uint32_t now_ms;
    uint32_t cmd_start_ms;
    uint32_t cmd_timeout_ms;
    char last_info[AT_LINE_MAX];
    at_cmd_cb_t cmd_cb;
    void *cmd_user;

    at_urc_cb_t urc_cb;
    at_ipd_cb_t ipd_cb;
    at_prompt_cb_t prompt_cb;
    void *cb_user;
};

void at_client_init(struct at_client *at,
                    struct ringbuf *rx_rb,
                    at_write_fn_t write_fn,
                    void *io_user);

void at_set_callbacks(struct at_client *at,
                      at_urc_cb_t urc_cb,
                      at_ipd_cb_t ipd_cb,
                      at_prompt_cb_t prompt_cb,
                      void *cb_user);

bool at_send(struct at_client *at,
             const char *cmd,
             uint32_t timeout_ms,
             at_cmd_cb_t cb,
             void *cmd_user);

void at_poll(struct at_client *at, uint32_t now_ms);

const char *at_result_str(enum at_result res);

#endif
