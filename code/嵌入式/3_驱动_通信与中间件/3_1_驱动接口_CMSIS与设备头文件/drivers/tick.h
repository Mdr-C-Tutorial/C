#ifndef DRIVERS_TICK_H
#define DRIVERS_TICK_H

#include <stdbool.h>
#include <stdint.h>

int tick_init(uint32_t sysclk_hz);
uint32_t tick_now(void);
bool tick_elapsed(uint32_t start, uint32_t interval_ms);

#endif
