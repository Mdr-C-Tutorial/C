#ifndef APP_BLINK_LOGIC_H
#define APP_BLINK_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

struct blink_ctx {
    uint32_t last_toggle_ms;
    uint32_t period_ms;
    bool led_on;
    bool prev_button;
};

void blink_logic_init(struct blink_ctx *ctx, uint32_t now_ms);
void blink_logic_step(struct blink_ctx *ctx, uint32_t now_ms);

#endif
