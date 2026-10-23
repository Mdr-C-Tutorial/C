/*
 * size_demo/retarget.c：把 newlib 的 printf 输出接到 USART1。
 *
 * printf() 最终调用 _write()。轮询发送，不依赖中断，6.2 的日志会用到同一个出口。
 *
 * 讲解见教程 6.1 节「固件架构、配置与资源预算」。
 */
#include <stdint.h>

#include "stm32f1xx.h"

int _write(int fd, const char *buf, int len) {
    (void)fd;
    for (int i = 0; i < len; ++i) {
        while ((USART1->SR & USART_SR_TXE) == 0u) {
        }
        USART1->DR = (uint8_t)buf[i];
    }
    while ((USART1->SR & USART_SR_TC) == 0u) {
    }
    return len;
}