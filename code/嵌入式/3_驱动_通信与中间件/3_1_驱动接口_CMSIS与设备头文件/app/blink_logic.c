#include "blink_logic.h"
#include "board.h"
#include "../drivers/tick.h"

void blink_logic_init(struct blink_ctx *ctx, uint32_t now_ms) {
    ctx->last_toggle_ms = now_ms;
    ctx->period_ms = 500u;
    ctx->led_on = false;
    ctx->prev_button = false;
    board_led_set(false);
}

void blink_logic_step(struct blink_ctx *ctx, uint32_t now_ms) {
    bool pressed = board_button_pressed();
    if (pressed && !ctx->prev_button) {
        if (ctx->period_ms == 500u) {
            ctx->period_ms = 100u;
            board_console_puts("mode: fast (100 ms)\n");
        } else {
            ctx->period_ms = 500u;
            board_console_puts("mode: slow (500 ms)\n");
        }
    }
    ctx->prev_button = pressed;

    if (tick_elapsed(ctx->last_toggle_ms, ctx->period_ms)) {
        ctx->led_on = !ctx->led_on;
        ctx->last_toggle_ms = now_ms;
        board_led_set(ctx->led_on);
    }
}
