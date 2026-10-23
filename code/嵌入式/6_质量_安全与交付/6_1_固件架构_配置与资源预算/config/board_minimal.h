#ifndef BOARD_MINIMAL_H
#define BOARD_MINIMAL_H

/* 自制的精简板：LED 接 PB12，高电平点亮；没有外部 Flash。 */
#define BOARD_NAME "minimal"
#define BOARD_LED_PORT 'B'
#define BOARD_LED_PIN 12u
#define BOARD_LED_ACTIVE_LOW 0
#define BOARD_HAS_W25Q64 0
#define BOARD_SYSCLK_HZ 72000000u

#endif /* BOARD_MINIMAL_H */
