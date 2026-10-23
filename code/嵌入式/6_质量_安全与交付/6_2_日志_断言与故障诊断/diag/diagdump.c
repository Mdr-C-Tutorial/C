#include <stdio.h>
#include <string.h>
#include "diag_record.h"

int main(void) {
    struct diag_record rec;
    struct diag_frame frame = {
        .r0 = 0x00000000u, .r1 = 0x20000400u, .r2 = 0x00000018u, .r3 = 0u,
        .r12 = 0u, .lr = 0x0800013Fu, .pc = 0x08000246u, .xpsr = 0x61000000u,
    };
    memset(&rec, 0, sizeof rec);
    rec.cfsr = 0x00008200u;
    rec.hfsr = 0x40000000u;
    rec.bfar = 0xCCCCCCCCu;
    for (uint16_t i = 0; i < 6u; ++i) {
        diag_crumb_push(&rec, (uint16_t)(0x10u + i), (uint16_t)i, 1000u + i * 25u);
    }
    diag_record_fill(&rec, DIAG_REASON_HARDFAULT, &frame, "sensor", 123456u);
    printf("magic=0x%08lX ver=%u reason=%u seq=%u\n", (unsigned long)rec.magic, rec.version, rec.reason, rec.seq);
    printf("pc=0x%08lX lr=0x%08lX cfsr=0x%08lX hfsr=0x%08lX bfar=0x%08lX\n",
           (unsigned long)rec.frame.pc, (unsigned long)rec.frame.lr,
           (unsigned long)rec.cfsr, (unsigned long)rec.hfsr, (unsigned long)rec.bfar);
    printf("task=%s uptime=%lu crumb_count=%u head=%u\n", rec.task,
           (unsigned long)rec.uptime_ms, rec.crumb_count, rec.crumb_head);
    for (uint8_t i = 0; i < rec.crumb_count; ++i) {
        const struct diag_crumb *c = &rec.crumbs[i];
        printf("  crumb[%u] t=%lu code=0x%02X arg=%u\n", i,
               (unsigned long)c->tick_ms, c->code, c->arg);
    }
    printf("crc32=0x%08lX valid=%d\n", (unsigned long)rec.crc32, (int)diag_record_valid(&rec));
    return 0;
}
