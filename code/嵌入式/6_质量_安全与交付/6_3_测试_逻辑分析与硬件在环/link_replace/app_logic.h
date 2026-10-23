/*
 * app_logic.h：只依赖 tick_now()，不知道它由谁实现。
 *
 * 讲解见教程 6.3 节「测试、逻辑分析与硬件在环」。
 */
#ifndef APP_LOGIC_H
#define APP_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

struct blink_ctx {
    uint32_t last_toggle_ms;
    uint32_t period_ms;
    bool led_on;
};

void blink_init(struct blink_ctx *ctx, uint32_t now_ms);
void blink_step(struct blink_ctx *ctx, uint32_t now_ms);

#endif /* APP_LOGIC_H */
