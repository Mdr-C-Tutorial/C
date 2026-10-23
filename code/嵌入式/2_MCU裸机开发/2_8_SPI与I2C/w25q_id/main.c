#include <stdint.h>
#include <stdio.h>
#include "clock.h"
#include "console.h"
#include "../spi.h"

int main(void) {
    clock_init_72mhz();
    console_init(72000000u, 115200u);
    spi1_init();

    uint8_t id[3] = {0};
    w25q_read_jedec_id(id);
    uint8_t sr1 = w25q_read_sr1();

    uint8_t head[8] = {0};
    w25q_read(0x000000u, head, sizeof head);

    printf("W25Q64 JEDEC ID: %02X %02X %02X, SR1: 0x%02X\n",
           id[0], id[1], id[2], sr1);
    printf("First 8 bytes:");
    for (size_t i = 0; i < sizeof head; ++i) {
        printf(" %02X", head[i]);
    }
    printf("\n");

    for (;;) {
    }
}
