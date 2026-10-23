#include "app_logic.h"

void blink_init(struct blink_ctx *ctx, uint32_t now_ms) {
    ctx->last_toggle_ms = now_ms;
    ctx->period_ms = 500u;
    ctx->led_on = false;
}

void blink_step(struct blink_ctx *ctx, uint32_t now_ms) {
    if ((uint32_t)(now_ms - ctx->last_toggle_ms) >= ctx->period_ms) {
        ctx->led_on = !ctx->led_on;
        ctx->last_toggle_ms = now_ms;
    }
}
