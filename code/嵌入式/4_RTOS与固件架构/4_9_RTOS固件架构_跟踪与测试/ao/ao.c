/*
 * ao.c：活动对象框架的实现。
 */
#include "ao.h"

static void ao_task(void *param) {
    ao_t *ao = (ao_t *)param;
    event_t ev;

    for (;;) {
        if (xQueueReceive(ao->queue, &ev, portMAX_DELAY) == pdPASS) {
            ao->handler(ao->self, &ev);
        }
    }
}

bool ao_start(ao_t *ao, const char *name, ao_handler_t handler, void *self,
              UBaseType_t priority, uint16_t depth) {
    ao->name = name;
    ao->handler = handler;
    ao->self = self;
    ao->queue = xQueueCreate(depth, sizeof(event_t));
    if (ao->queue == NULL) {
        return false;
    }
    if (xTaskCreate(ao_task, name, 512, ao, priority, &ao->task) != pdPASS) {
        vQueueDelete(ao->queue);
        ao->queue = NULL;
        return false;
    }
    return true;
}

bool ao_post(ao_t *ao, uint16_t sig, intptr_t param, TickType_t wait) {
    event_t ev = { .sig = sig, .param = param };

    return xQueueSend(ao->queue, &ev, wait) == pdPASS;
}

bool ao_post_from_isr(ao_t *ao, uint16_t sig, intptr_t param) {
    event_t ev = { .sig = sig, .param = param };
    BaseType_t woken = pdFALSE;

    if (xQueueSendFromISR(ao->queue, &ev, &woken) != pdPASS) {
        return false;
    }
    portYIELD_FROM_ISR(woken);
    return true;
}

void ao_dispatch(ao_t *ao, const event_t *ev) {
    ao->handler(ao->self, ev);
}
