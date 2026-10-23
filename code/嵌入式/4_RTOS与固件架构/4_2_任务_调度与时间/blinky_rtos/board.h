#ifndef BOARD_H
#define BOARD_H

/* 把 SYSCLK 配到 72 MHz，并使能 GPIOC 的时钟。 */
void board_init(void);

/* 翻转 PC13 上的板载 LED（低电平点亮）。 */
void led_toggle(void);

#endif /* BOARD_H */
