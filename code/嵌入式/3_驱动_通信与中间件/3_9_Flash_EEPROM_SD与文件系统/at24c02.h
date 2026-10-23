#ifndef AT24C02_H
#define AT24C02_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AT24C02_I2C_ADDR  0x50u
#define AT24C02_TOTAL_LEN 256u
#define AT24C02_PAGE_SIZE 8u

bool at24c02_wait_ready(uint32_t max_tries);
bool at24c02_write_page(uint8_t word_addr, const uint8_t *data, size_t len);
bool at24c02_write(uint8_t word_addr, const void *buf, size_t len);
bool at24c02_read(uint8_t word_addr, void *buf, size_t len);

#endif /* AT24C02_H */
