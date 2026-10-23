/*
 * led_fsm.c：LED 状态机的实现。每次按键推进 OFF -> ON -> BLINK -> OFF，
 * 每 100 ms 的 TICK 用来在 BLINK 状态下翻转灯。
 */
#include "led_fsm.h"

#define BLINK_HALF_PERIOD_TICKS 5u

void led_fsm_init(struct led_fsm *fsm) {
    fsm->state = LED_OFF;
    fsm->ticks_in_state = 0;
    fsm->transitions = 0;
}

bool led_fsm_dispatch(struct led_fsm *fsm, const event_t *ev) {
    bool changed = false;

    switch (ev->sig) {
    case SIG_LED_BUTTON:
        fsm->state = (enum led_state)((fsm->state + 1) % 3);
        fsm->ticks_in_state = 0;
        fsm->transitions++;
        changed = true;
        break;

    case SIG_LED_TICK:
        fsm->ticks_in_state++;
        if (fsm->state == LED_BLINK &&
            fsm->ticks_in_state % BLINK_HALF_PERIOD_TICKS == 0) {
            changed = true;
        }
        break;

    default:
        break;
    }

    return changed;
}

const char *led_state_name(enum led_state state) {
    switch (state) {
    case LED_OFF:
        return "OFF";
    case LED_ON:
        return "ON";
    case LED_BLINK:
        return "BLINK";
    default:
        return "?";
    }
}
