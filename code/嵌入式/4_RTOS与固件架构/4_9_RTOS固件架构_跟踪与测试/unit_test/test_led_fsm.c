/*
 * test_led_fsm.c：LED 状态机的主机单元测试，不启动调度器。
 */
#include <stdio.h>
#include <string.h>

#include "led_fsm.h"

static int g_failures;
static int g_checks;

#define CHECK(cond)                                                          \
    do {                                                                     \
        g_checks++;                                                          \
        if (!(cond)) {                                                       \
            g_failures++;                                                    \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                    \
    } while (0)

static void feed(struct led_fsm *fsm, uint16_t sig, int times) {
    for (int i = 0; i < times; ++i) {
        event_t ev = { .sig = sig, .param = 0 };
        led_fsm_dispatch(fsm, &ev);
    }
}

static void test_button_cycles(void) {
    struct led_fsm fsm;

    led_fsm_init(&fsm);
    CHECK(fsm.state == LED_OFF);

    feed(&fsm, SIG_LED_BUTTON, 1);
    CHECK(fsm.state == LED_ON);
    feed(&fsm, SIG_LED_BUTTON, 1);
    CHECK(fsm.state == LED_BLINK);
    feed(&fsm, SIG_LED_BUTTON, 1);
    CHECK(fsm.state == LED_OFF);
    CHECK(fsm.transitions == 3);
}

static void test_blink_toggles_every_five_ticks(void) {
    struct led_fsm fsm;

    led_fsm_init(&fsm);
    feed(&fsm, SIG_LED_BUTTON, 2); /* -> BLINK */
    CHECK(fsm.state == LED_BLINK);

    int toggles = 0;
    for (int i = 0; i < 20; ++i) {
        event_t ev = { .sig = SIG_LED_TICK, .param = 0 };
        if (led_fsm_dispatch(&fsm, &ev)) {
            toggles++;
        }
    }
    CHECK(toggles == 4);
}

static void test_tick_in_off_state_is_ignored(void) {
    struct led_fsm fsm;

    led_fsm_init(&fsm);
    CHECK(!led_fsm_dispatch(&fsm, &(event_t){ .sig = SIG_LED_TICK }));
    CHECK(fsm.state == LED_OFF);
    CHECK(fsm.transitions == 0);
}

static void test_unknown_signal_keeps_state(void) {
    struct led_fsm fsm;

    led_fsm_init(&fsm);
    CHECK(!led_fsm_dispatch(&fsm, &(event_t){ .sig = 99 }));
    CHECK(fsm.state == LED_OFF);
}

int main(void) {
    test_button_cycles();
    test_blink_toggles_every_five_ticks();
    test_tick_in_off_state_is_ignored();
    test_unknown_signal_keeps_state();

    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
