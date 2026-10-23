#ifndef FAULT_H
#define FAULT_H

#include <stdint.h>

typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
} fault_frame_t;

typedef struct {
    uint32_t magic;
    uint32_t pc;
    uint32_t lr;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t bfar;
    uint32_t checksum;
} crash_record_t;

#define CRASH_MAGIC 0x4641554Cu /* "FAUL" */

void fault_enable_traps(void);
void hardfault_c(const fault_frame_t *frame);
int crash_record_load(crash_record_t *out);
void crash_record_clear(void);

#endif /* FAULT_H */
