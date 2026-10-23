/*
 * board.h：编译检查用的最小板级头文件，只提供 GET_PIN 需要的 GPIO 基址。
 *
 * 真实 blue-pill BSP 的 board.h 还会定义 STM32_FLASH_SIZE、HEAP_BEGIN、HEAP_END，
 * 并声明 SystemClock_Config()。
 */
#ifndef __BOARD_H__
#define __BOARD_H__

#define GPIOA (0x40010800UL)
#define GPIOB (0x40010C00UL)
#define GPIOC (0x40011000UL)
#define GPIOD (0x40011400UL)
#define GPIOE (0x40011800UL)

/* drv_gpio.h 里的写法用端口基址做偏移，这里简化成直接相加 */
#define GET_PIN(PORTx, PIN) (rt_base_t)((rt_base_t)GPIO##PORTx + (PIN))

#endif /* __BOARD_H__ */