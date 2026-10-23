/*
 * led_ao.c：把 LED 状态机装进活动对象。这里不出现任何 FreeRTOS 调用。
 */
#include "ao.h"
#include "led_fsm.h"
#include "led_ao.h"
#include "rtos_console.h"

struct led_ao {
    struct led_fsm fsm;
    bool lamp_on;
};

static struct led_ao g_led;

static void led_ao_handle(void *self, const event_t *ev) {
    struct led_ao *led = (struct led_ao *)self;
    bool changed = led_fsm_dispatch(&led->fsm, ev);

    if (!changed) {
        return;
    }

    if (led->fsm.state == LED_BLINK) {
        led->lamp_on = !led->lamp_on;
        console_printf("[led] BLINK lamp=%s (tick %lu)\n",
                       led->lamp_on ? "on" : "off",
                       (unsigned long)xTaskGetTickCount());
    } else {
        led->lamp_on = led->fsm.state == LED_ON;
        console_printf("[led] state=%s lamp=%s (tick %lu)\n",
                       led_state_name(led->fsm.state),
                       led->lamp_on ? "on" : "off",
                       (unsigned long)xTaskGetTickCount());
    }
}

void led_ao_init(ao_t *ao, UBaseType_t priority) {
    led_fsm_init(&g_led.fsm);
    g_led.lamp_on = false;
    ao_start(ao, "led", led_ao_handle, &g_led, priority, 8);
}

struct led_view led_ao_state(void) {
    struct led_view view = {
        .lamp_on = g_led.lamp_on,
        .transitions = g_led.fsm.transitions,
        .state = g_led.fsm.state,
    };

    return view;
}
