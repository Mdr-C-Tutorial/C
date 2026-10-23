#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

#include "i2c_bus.h"

/*
 * 板级支持：只做“板子上有什么、接在哪个引脚”的决定。
 * 应用层通过这里的函数拿总线，不直接碰 GPIO 寄存器。
 */
void board_init(void);
void board_led_set(int on);
void board_led_toggle(void);

/* 串口命令行：初始化 USART1 接收中断，取一行（不带换行）。 */
void board_console_init(void);
/* 返回一行文本并去掉行尾的 \r\n；没有完整行时返回 0。 */
int board_console_try_read_line(char *buf, int max);

const i2c_bus_t *board_i2c_bus(void);

uint32_t board_tick_now(void);

/* 启动横幅里打印的复位原因（2.3 的 reset_cause_get 在这里的等价实现） */
const char *board_reset_cause_name(void);

#endif /* BOARD_H */