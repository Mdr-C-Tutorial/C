/*
 * basepri_nest.c：嵌套临界区里“直接写 BASEPRI”与“只提高不降低”的指令对比。
 *
 * 编译与反汇编：
 *   arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra  *       -DSTM32F103xB -I<cmsis 目录> -c basepri_nest.c
 *   arm-none-eabi-objdump -d basepri_nest.o
 */
#include <stdint.h>
#include "stm32f1xx.h"

#define LOG_MASK_PRIO (5u << (8u - __NVIC_PRIO_BITS))

/* 错误：每次都写 BASEPRI，内层退出时把外层设的更高级屏蔽放开了 */
void bad_nesting(void) {
    uint32_t outer = __get_BASEPRI();
    __set_BASEPRI(LOG_MASK_PRIO);
    {
        uint32_t inner = __get_BASEPRI();
        __set_BASEPRI(9u << (8u - __NVIC_PRIO_BITS)); /* 内层屏蔽得更宽 */
        __set_BASEPRI(inner);
    }
    __set_BASEPRI(outer);
}

/* 正确：进入只提高不降低，退出恢复进入前的值 */
void good_nesting(void) {
    uint32_t outer = __get_BASEPRI();
    __set_BASEPRI_MAX(LOG_MASK_PRIO);
    {
        uint32_t inner = __get_BASEPRI();
        __set_BASEPRI_MAX(9u << (8u - __NVIC_PRIO_BITS));
        __set_BASEPRI(inner);
    }
    __set_BASEPRI(outer);
}
