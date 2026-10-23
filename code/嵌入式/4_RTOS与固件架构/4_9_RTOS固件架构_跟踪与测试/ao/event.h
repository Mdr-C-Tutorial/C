/*
 * event.h：活动对象之间传递的事件。只有信号与一个整型参数，没有指针到可变数据。
 */
#ifndef EVENT_H
#define EVENT_H

#include <stdint.h>

typedef struct {
    uint16_t sig;
    intptr_t param;
} event_t;

#endif /* EVENT_H */
