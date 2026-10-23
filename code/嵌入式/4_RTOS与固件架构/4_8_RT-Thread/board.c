/*
 * board.c：最小板级支持，只提供时钟节拍与控制台输出的空实现，供编译检查。
 *
 * 真实的 blue-pill BSP 里这一份由 CubeMX 生成的 SystemClock_Config 与
 * drv_usart、drv_gpio 等驱动组成，见 bsp/stm32/stm32f103-blue-pill/board/。
 */
#include <rthw.h>
#include <rtthread.h>

#define SYST_CSR (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018u)
#define SYSTICK_FREQ 72000000u

void rt_hw_board_init(void) {
    SYST_RVR = SYSTICK_FREQ / RT_TICK_PER_SECOND - 1u;
    SYST_CVR = 0u;
    SYST_CSR = 0x7u;
}

/* 节拍中断：进入/退出中断上下文并累加节拍，这是移植层的职责 */
void SysTick_Handler(void) {
    rt_interrupt_enter();
    rt_tick_increase();
    rt_interrupt_leave();
}

/* 控制台：正式 BSP 里由 uart 设备接出去，这里只保留接口 */
void rt_hw_console_output(const char *str) {
    (void)str;
}

char rt_hw_console_getchar(void) {
    return -1;
}