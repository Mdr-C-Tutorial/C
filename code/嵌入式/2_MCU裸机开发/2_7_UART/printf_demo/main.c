#include <stdio.h>
#include <stdint.h>
#include "clock.h"
#include "console.h"

int main(void) {
    clock_init_72mhz();
    console_init(72000000u, 115200u);

    uint32_t seq = 0;
    int32_t temp_mdeg = 25375;

    printf("Boot ok, SYSCLK = %lu Hz\n", 72000000UL);
    printf("seq=%lu, temp=%ld.%03ld C\n",
           (unsigned long)seq,
           (long)(temp_mdeg / 1000),
           (long)(temp_mdeg % 1000));

    for (;;) {
    }
}
