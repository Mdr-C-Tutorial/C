#include "at_parser.h"
#include <string.h>

static bool starts_with(const char *str, const char *prefix) {
    size_t n = strlen(prefix);
    return strncmp(str, prefix, n) == 0;
}

static bool is_urc_line(const char *line) {
    static const char *const urc_prefixes[] = {
        "ready",
        "WIFI CONNECTED",
        "WIFI GOT IP",
        "WIFI DISCONNECT",
        "CONNECT",
        "CLOSED",
        "+BLECONN:",
        "+BLEDISCONN:",
        "+WRITE:",
    };
    size_t count = sizeof(urc_prefixes) / sizeof(urc_prefixes[0]);
    for (size_t i = 0u; i < count; ++i) {
        if (starts_with(line, urc_prefixes[i])) {
            return true;
        }
    }
    return false;
}

static void finish_command(struct at_client *at, enum at_result res) {
    at_cmd_cb_t cb = at->cmd_cb;
    void *user = at->cmd_user;
    at->pending = false;
    at->expect_prompt = false;
    at->prompt_seen = false;
    at->cmd_cb = NULL;
    at->cmd_user = NULL;
    if (cb != NULL) {
        cb(res, at->last_info, user);
    }
}

static void handle_line(struct at_client *at, const char *line) {
    if (at->pending) {
        if (strcmp(line, "OK") == 0) {
            /* AT+CIPSEND 先回 OK，再单独给出 '>' 提示；此处等提示符，不结束命令 */
            if (at->expect_prompt && !at->prompt_seen) {
                return;
            }
            finish_command(at, AT_RES_OK);
            return;
        }
        if (strcmp(line, "SEND OK") == 0) {
            finish_command(at, AT_RES_OK);
            return;
        }
        if (strcmp(line, "ERROR") == 0 || strcmp(line, "SEND FAIL") == 0) {
            finish_command(at, AT_RES_ERROR);
            return;
        }
        if (starts_with(line, "busy p")) {
            finish_command(at, AT_RES_BUSY);
            return;
        }
        if (is_urc_line(line)) {
            if (at->urc_cb != NULL) {
                at->urc_cb(line, at->cb_user);
            }
            return;
        }
        strncpy(at->last_info, line, AT_LINE_MAX - 1u);
        at->last_info[AT_LINE_MAX - 1u] = '\0';
        return;
    }

    if (at->urc_cb != NULL) {
        at->urc_cb(line, at->cb_user);
    }
}

static void feed_byte(struct at_client *at, uint8_t b) {
    if (at->state == AT_RX_IPD_LEN) {
        if (b >= (uint8_t)'0' && b <= (uint8_t)'9') {
            at->ipd_expected =
                at->ipd_expected * 10u + (size_t)(b - (uint8_t)'0');
            if (at->ipd_expected > AT_IPD_MAX) {
                at->state = AT_RX_LINE;
                at->line_len = 0u;
            }
        } else if (b == (uint8_t)':') {
            if (at->ipd_expected > 0u && at->ipd_expected <= AT_IPD_MAX) {
                at->ipd_received = 0u;
                at->state = AT_RX_IPD_DATA;
            } else {
                at->state = AT_RX_LINE;
                at->line_len = 0u;
            }
        } else {
            at->state = AT_RX_LINE;
            at->line_len = 0u;
        }
        return;
    }

    if (at->state == AT_RX_IPD_DATA) {
        at->ipd_buf[at->ipd_received++] = b;
        if (at->ipd_received == at->ipd_expected) {
            size_t len = at->ipd_expected;
            at->state = AT_RX_LINE;
            at->line_len = 0u;
            if (at->ipd_cb != NULL) {
                at->ipd_cb(at->ipd_buf, len, at->cb_user);
            }
        }
        return;
    }

    /* AT_RX_LINE：按行装配，同时识别不带换行的 '>' 与 '+IPD,' 前缀 */
    if (b == (uint8_t)'\r') {
        return;
    }
    if (b == (uint8_t)'\n') {
        if (at->line_len > 0u) {
            at->line_buf[at->line_len] = '\0';
            at->line_len = 0u;
            handle_line(at, at->line_buf);
        }
        return;
    }

    if (at->line_len < AT_LINE_MAX - 1u) {
        at->line_buf[at->line_len++] = (char)b;
    }

    if (at->pending && at->line_len == 1u && at->line_buf[0] == '>') {
        at->line_len = 0u;
        if (at->expect_prompt) {
            at->prompt_seen = true;
        }
        if (at->prompt_cb != NULL) {
            at->prompt_cb(at->cb_user);
        }
        return;
    }

    if (at->line_len == 5u && memcmp(at->line_buf, "+IPD,", 5u) == 0) {
        at->line_len = 0u;
        at->ipd_expected = 0u;
        at->ipd_received = 0u;
        at->state = AT_RX_IPD_LEN;
    }
}

void at_client_init(struct at_client *at,
                    struct ringbuf *rx_rb,
                    at_write_fn_t write_fn,
                    void *io_user) {
    *at = (struct at_client){0};
    at->rx_rb = rx_rb;
    at->write_fn = write_fn;
    at->io_user = io_user;
    at->state = AT_RX_LINE;
}

void at_set_callbacks(struct at_client *at,
                      at_urc_cb_t urc_cb,
                      at_ipd_cb_t ipd_cb,
                      at_prompt_cb_t prompt_cb,
                      void *cb_user) {
    at->urc_cb = urc_cb;
    at->ipd_cb = ipd_cb;
    at->prompt_cb = prompt_cb;
    at->cb_user = cb_user;
}

bool at_send(struct at_client *at,
             const char *cmd,
             uint32_t timeout_ms,
             at_cmd_cb_t cb,
             void *cmd_user) {
    if (at == NULL || cmd == NULL || at->pending) {
        return false;
    }
    at->pending = true;
    at->expect_prompt = starts_with(cmd, "AT+CIPSEND") ||
                        starts_with(cmd, "AT+BLEGATTSNTFY");
    at->prompt_seen = false;
    at->cmd_start_ms = at->now_ms;
    at->cmd_timeout_ms = timeout_ms;
    at->last_info[0] = '\0';
    at->cmd_cb = cb;
    at->cmd_user = cmd_user;

    if (at->write_fn != NULL) {
        at->write_fn((const uint8_t *)cmd, strlen(cmd), at->io_user);
        at->write_fn((const uint8_t *)"\r\n", 2u, at->io_user);
    }
    return true;
}

void at_poll(struct at_client *at, uint32_t now_ms) {
    at->now_ms = now_ms;

    uint8_t byte = 0u;
    while (rb_get(at->rx_rb, &byte)) {
        feed_byte(at, byte);
    }

    if (at->pending &&
        (uint32_t)(now_ms - at->cmd_start_ms) >= at->cmd_timeout_ms) {
        finish_command(at, AT_RES_TIMEOUT);
    }
}

const char *at_result_str(enum at_result res) {
    switch (res) {
    case AT_RES_OK:
        return "OK";
    case AT_RES_ERROR:
        return "ERROR";
    case AT_RES_BUSY:
        return "BUSY";
    case AT_RES_TIMEOUT:
        return "TIMEOUT";
    default:
        return "UNKNOWN";
    }
}
