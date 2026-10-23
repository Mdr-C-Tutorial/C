#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "app/blink_logic.h"
#include "app/board.h"
#include "drivers/tick.h"

static uint32_t g_sim_ms;
static bool g_sim_btn;
static bool g_sim_led;

int tick_init(uint32_t sysclk_hz) {
    (void)sysclk_hz;
    g_sim_ms = 0u;
    return 0;
}

uint32_t tick_now(void) {
    return g_sim_ms;
}

bool tick_elapsed(uint32_t start, uint32_t interval_ms) {
    return (uint32_t)(g_sim_ms - start) >= interval_ms;
}

void board_init(void) {
    tick_init(8000000u);
    g_sim_btn = false;
    g_sim_led = false;
}

void board_led_set(bool on) {
    g_sim_led = on;
    printf("[t=%4u ms] LED -> %s\n", g_sim_ms, on ? "ON" : "OFF");
}

bool board_button_pressed(void) {
    return g_sim_btn;
}

void board_console_puts(const char *s) {
    printf("[t=%4u ms] UART: %s", g_sim_ms, s);
}

int main(void) {
    board_init();

    struct blink_ctx ctx;
    blink_logic_init(&ctx, tick_now());

    for (uint32_t t = 50u; t <= 800u; t += 50u) {
        g_sim_ms = t;
        /* 在 550 ms 按下按键，650 ms 松开 */
        g_sim_btn = (t >= 550u && t < 650u);
        blink_logic_step(&ctx, tick_now());
    }
    return 0;
}
