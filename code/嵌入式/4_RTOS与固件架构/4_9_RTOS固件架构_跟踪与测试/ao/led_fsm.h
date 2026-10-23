/*
 * led_fsm.h：LED 状态机，纯逻辑，不依赖任何 RTOS。
 */
#ifndef LED_FSM_H
#define LED_FSM_H

#include <stdbool.h>
#include <stdint.h>

#include "event.h"

enum {
    SIG_LED_BUTTON = 1,
    SIG_LED_TICK,
};

enum led_state {
    LED_OFF = 0,
    LED_ON,
    LED_BLINK,
};

struct led_fsm {
    enum led_state state;
    uint32_t ticks_in_state;
    uint32_t transitions;
};

void led_fsm_init(struct led_fsm *fsm);

/* 返回状态是否改变，调用方据此决定要不要打印。 */
bool led_fsm_dispatch(struct led_fsm *fsm, const event_t *ev);

const char *led_state_name(enum led_state state);

#endif /* LED_FSM_H */
