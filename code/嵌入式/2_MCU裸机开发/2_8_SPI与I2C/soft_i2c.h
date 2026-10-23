#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AT24C02_ADDR 0x50u

void i2c_init(void);
void i2c_start(void);
void i2c_stop(void);
bool i2c_write_byte(uint8_t byte);
uint8_t i2c_read_byte(bool ack);

bool at24c02_write_byte(uint8_t word_addr, uint8_t data);
bool at24c02_wait_ready(uint32_t max_tries);
bool at24c02_read_byte(uint8_t word_addr, uint8_t *data);
bool at24c02_read(uint8_t word_addr, void *buf, size_t len);

#endif /* SOFT_I2C_H */
