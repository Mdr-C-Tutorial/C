#include <stdint.h>
#include <stdio.h>
#include "clock.h"
#include "console.h"
#include "../soft_i2c.h"

int main(void) {
    clock_init_72mhz();
    console_init(72000000u, 115200u);
    i2c_init();

    uint8_t addr = 0x10u;
    uint8_t tx_val = 0xA5u;
    uint8_t rx_val = 0x00u;

    if (at24c02_write_byte(addr, tx_val) &&
        at24c02_wait_ready(1000u) &&
        at24c02_read_byte(addr, &rx_val)) {
        printf("AT24C02[0x%02X]: wrote 0x%02X, read 0x%02X\n",
               addr, tx_val, rx_val);
    } else {
        printf("AT24C02 transfer failed\n");
    }

    for (;;) {
    }
}
