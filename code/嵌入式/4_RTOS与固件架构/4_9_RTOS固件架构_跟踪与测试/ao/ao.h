/*
 * ao.h：用 FreeRTOS 队列实现的最小活动对象框架。
 *
 * 每个活动对象有自己的私有数据、一个事件队列和一个任务；对象之间只投递
 * 事件，不共享数据，所以不需要互斥量。
 */
#ifndef AO_H
#define AO_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "event.h"

typedef struct ao ao_t;
typedef void (*ao_handler_t)(void *self, const event_t *ev);

struct ao {
    const char *name;
    QueueHandle_t queue;
    TaskHandle_t task;
    ao_handler_t handler;
    void *self;
};

/* 建队列并创建任务；任务名与对象名相同。 */
bool ao_start(ao_t *ao, const char *name, ao_handler_t handler, void *self,
              UBaseType_t priority, uint16_t depth);

/* 从任务里投递事件；队列满时最多等 wait 个节拍。 */
bool ao_post(ao_t *ao, uint16_t sig, intptr_t param, TickType_t wait);

/* 从模拟中断里投递事件。 */
bool ao_post_from_isr(ao_t *ao, uint16_t sig, intptr_t param);

/* 直接分发一个事件，不经过队列；单元测试用。 */
void ao_dispatch(ao_t *ao, const event_t *ev);

#endif /* AO_H */
