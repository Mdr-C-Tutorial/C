#include <string.h>
#include "app/blink_logic.h"
#include "app/board.h"
#include "drivers/gpio.h"
#include "drivers/tick.h"
#include "drivers/uart.h"
#include "stm32f1xx.h"

/* Blue Pill 板级引脚：PC13 低电平亮，PA0 按键按下为高电平 */
static const struct gpio_pin k_led_pin = { GPIOC, 13u, true };
static const struct gpio_pin k_btn_pin = { GPIOA, 0u, false };
static const struct gpio_pin k_tx_pin  = { GPIOA, 9u, false };
static const struct gpio_pin k_rx_pin  = { GPIOA, 10u, false };

void board_init(void) {
    SystemCoreClockUpdate();
    tick_init(SystemCoreClock);

    gpio_pin_init(&k_led_pin, GPIO_MODE_OUT_PP_2MHZ);
    gpio_pin_init(&k_btn_pin, GPIO_MODE_IN_PULL_DOWN);

    SET_BIT(RCC->APB2ENR, RCC_APB2ENR_USART1EN);
    gpio_pin_init(&k_tx_pin, GPIO_MODE_AF_PP_50MHZ);
    gpio_pin_init(&k_rx_pin, GPIO_MODE_IN_FLOATING);

    struct uart_config cfg = { .baudrate = 115200u };
    uart_init(USART1, SystemCoreClock, &cfg);
}

void board_led_set(bool on) {
    gpio_pin_write(&k_led_pin, on);
}

bool board_button_pressed(void) {
    return gpio_pin_read(&k_btn_pin);
}

void board_console_puts(const char *s) {
    uart_write(USART1, s, strlen(s));
}

int main(void) {
    board_init();

    struct blink_ctx ctx;
    blink_logic_init(&ctx, tick_now());

    for (;;) {
        blink_logic_step(&ctx, tick_now());
    }
}
