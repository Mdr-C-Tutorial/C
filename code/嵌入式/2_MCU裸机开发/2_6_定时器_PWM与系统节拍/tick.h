#ifndef TICK_H
#define TICK_H

#include <stdint.h>

void tick_init(void);
uint32_t tick_now(void);
int tick_elapsed(uint32_t start, uint32_t interval_ms);
void delay_ms(uint32_t ms);

#endif /* TICK_H */
