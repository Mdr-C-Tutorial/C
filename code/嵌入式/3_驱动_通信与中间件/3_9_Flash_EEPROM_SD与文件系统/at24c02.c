#include "at24c02.h"

/* 复用 2.8 节 soft_i2c.c 的底层 I2C 原语与应答轮询 */
extern void i2c_start(void);
extern void i2c_stop(void);
extern bool i2c_write_byte(uint8_t byte);

/* 单页写入：调用者须保证 [word_addr, word_addr + len) 不跨越 8 字节页边界 */
bool at24c02_write_page(uint8_t word_addr, const uint8_t *data, size_t len) {
    if (len == 0u || len > AT24C02_PAGE_SIZE) {
        return false;
    }
    if (((word_addr & 0x07u) + len) > AT24C02_PAGE_SIZE) {
        return false;
    }

    i2c_start();
    if (!i2c_write_byte((uint8_t)((AT24C02_I2C_ADDR << 1) | 0u)) ||
        !i2c_write_byte(word_addr)) {
        i2c_stop();
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if (!i2c_write_byte(data[i])) {
            i2c_stop();
            return false;
        }
    }
    i2c_stop();

    /* 发送停止条件后，器件启动内部写周期（最长 5 ms），通过应答轮询等待完成 */
    return at24c02_wait_ready(1000u);
}

/* 任意长度写入：按 8 字节页边界自动拆分 */
bool at24c02_write(uint8_t word_addr, const void *buf, size_t len) {
    if ((size_t)word_addr + len > AT24C02_TOTAL_LEN) {
        return false;
    }
    const uint8_t *src = (const uint8_t *)buf;
    while (len > 0u) {
        size_t page_rem = AT24C02_PAGE_SIZE - (word_addr & (AT24C02_PAGE_SIZE - 1u));
        size_t chunk = (len < page_rem) ? len : page_rem;
        if (!at24c02_write_page(word_addr, src, chunk)) {
            return false;
        }
        word_addr = (uint8_t)(word_addr + chunk);
        src += chunk;
        len -= chunk;
    }
    return true;
}
