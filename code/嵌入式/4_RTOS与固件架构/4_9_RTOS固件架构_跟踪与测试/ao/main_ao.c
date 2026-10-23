/*
 * main_ao.c：按键、心跳、LED 三个活动对象协作。
 *
 * 按键由模拟中断产生，中断里只投递事件；心跳任务每 100 ms 给 LED 发 TICK；
 * 三个对象之间不共享任何对象，也没有互斥量。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "ao.h"
#include "led_fsm.h"
#include "led_ao.h"
#include "rtos_console.h"

#define BUTTON_IRQ_APP 2u

static ao_t g_led_ao;
static ao_t g_button_ao;
static ao_t g_tick_ao;

/* ---- 按键活动对象：把中断事件记成按键数 ---- */
struct button_ao {
    uint32_t presses;
};

static struct button_ao g_button;

static void button_handle(void *self, const event_t *ev) {
    struct button_ao *b = (struct button_ao *)self;

    if (ev->sig == SIG_LED_BUTTON) {
        b->presses++;
        console_printf("[button] press #%lu -> post to led\n",
                       (unsigned long)b->presses);
        ao_post(&g_led_ao, SIG_LED_BUTTON, 0, 0);
    }
}

static uint32_t button_isr(void) {
    ao_post_from_isr(&g_button_ao, SIG_LED_BUTTON, 0);
    return 0;
}

/* ---- 心跳活动对象：每 100 ms 产生一次 TICK ---- */
static void tick_handle(void *self, const event_t *ev) {
    (void)self;
    if (ev->sig == SIG_LED_TICK) {
        ao_post(&g_led_ao, SIG_LED_TICK, 0, 0);
    }
}

static void tick_generator_task(void *param) {
    (void)param;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        ao_post(&g_tick_ao, SIG_LED_TICK, 0, portMAX_DELAY);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
    }
}

/* ---- 模拟按键：每 700 ms 产生一次抖动后的按下 ---- */
static void button_sim_task(void *param) {
    struct led_view view;

    (void)param;
    for (unsigned round = 0; round < 4; ++round) {
        vTaskDelay(pdMS_TO_TICKS(700));
        console_printf("--- simulated press %u ---\n", round + 1);
        vPortGenerateSimulatedInterrupt(BUTTON_IRQ_APP);
    }

    vTaskDelay(pdMS_TO_TICKS(400));
    view = led_ao_state();
    console_printf("[main] done: presses=%lu transitions=%lu state=%s\n",
                   (unsigned long)g_button.presses,
                   (unsigned long)view.transitions,
                   led_state_name(view.state));
    sim_finish();
}

int main(void) {
    console_init();
    vPortSetInterruptHandler(BUTTON_IRQ_APP, button_isr);

    led_ao_init(&g_led_ao, 3);
    ao_start(&g_button_ao, "button", button_handle, &g_button, 4, 8);
    ao_start(&g_tick_ao, "tick", tick_handle, NULL, 2, 4);

    xTaskCreate(tick_generator_task, "tickgen", 512, NULL, 1, NULL);
    xTaskCreate(button_sim_task, "btnsim", 512, NULL, 5, NULL);

    vTaskStartScheduler();
    return 0;
}
