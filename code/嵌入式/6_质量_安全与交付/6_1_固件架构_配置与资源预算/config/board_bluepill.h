#ifndef BOARD_BLUEPILL_H
#define BOARD_BLUEPILL_H

/* Blue Pill：LED 接 PC13，低电平点亮；板上有 W25Q64（SPI1 + PA4 片选）。 */
#define BOARD_NAME "bluepill"
#define BOARD_LED_PORT 'C'
#define BOARD_LED_PIN 13u
#define BOARD_LED_ACTIVE_LOW 1
#define BOARD_HAS_W25Q64 1
#define BOARD_SYSCLK_HZ 72000000u

#endif /* BOARD_BLUEPILL_H */
