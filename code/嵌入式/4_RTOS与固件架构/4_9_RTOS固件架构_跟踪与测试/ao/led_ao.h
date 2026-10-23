/*
 * led_ao.h：LED 活动对象的对外接口。
 */
#ifndef LED_AO_H
#define LED_AO_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "ao.h"
#include "led_fsm.h"

struct led_view {
    bool lamp_on;
    uint32_t transitions;
    enum led_state state;
};

void led_ao_init(ao_t *ao, UBaseType_t priority);

/* 读一份状态快照，供示例收尾时打印；真实工程里用事件把结果发出去 */
struct led_view led_ao_state(void);

#endif /* LED_AO_H */
