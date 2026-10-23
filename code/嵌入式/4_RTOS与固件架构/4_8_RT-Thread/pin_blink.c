/*
 * pin_blink.c：用 PIN 设备闪烁 PC13（blue-pill 板载 LED）。
 *
 * PIN 设备由 drv_gpio.c 注册进 I/O 设备框架，rt_pin_* 最终调到 BSP 的寄存器操作。
 * GET_PIN 来自 drv_gpio.h，C 对应 GPIOC，13 是引脚号。
 */
#include <rtthread.h>
#include <rtdevice.h>

#include "board.h"

#define LED_PIN GET_PIN(C, 13)

static void blink_thread(void *param) {
    (void)param;

    rt_pin_mode(LED_PIN, PIN_MODE_OUTPUT);
    for (;;) {
        rt_pin_write(LED_PIN, PIN_HIGH);
        rt_thread_mdelay(500);
        rt_pin_write(LED_PIN, PIN_LOW);
        rt_thread_mdelay(500);
    }
}

/* 线程在 APP 初始化阶段创建：这时调度器已经启动，mdelay 可用 */
static int blink_init(void) {
    rt_thread_t tid = rt_thread_create("blink", blink_thread, RT_NULL, 512, 20, 10);

    if (tid != RT_NULL) {
        rt_thread_startup(tid);
    }
    return RT_EOK;
}
INIT_APP_EXPORT(blink_init);