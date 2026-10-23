#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

extern uint32_t SystemCoreClock;

typedef enum {
    RESET_CAUSE_UNKNOWN = 0,
    RESET_CAUSE_LOW_POWER,
    RESET_CAUSE_WWDG,
    RESET_CAUSE_IWDG,
    RESET_CAUSE_SOFTWARE,
    RESET_CAUSE_POR,
    RESET_CAUSE_PIN
} reset_cause_t;

void clock_init_72mhz(void);
void mco_enable_pll_div2(void);
reset_cause_t reset_cause_get(void);
const char *reset_cause_name(reset_cause_t cause);

#endif /* CLOCK_H */
