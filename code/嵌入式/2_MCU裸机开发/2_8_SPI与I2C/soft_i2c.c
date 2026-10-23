#include "soft_i2c.h"
#include "stm32f1xx.h"

static void i2c_delay_half(void) {
    /* 72 MHz 下约 5 us 半周期（对应约 100 kHz SCL） */
    for (volatile uint32_t i = 0; i < 45u; ++i) {
        __NOP();
    }
}

static inline void scl_high(void) {
    GPIOB->BSRR = GPIO_BSRR_BS6;
}

static inline void scl_low(void) {
    GPIOB->BSRR = GPIO_BSRR_BR6;
}

static inline void sda_high(void) {
    GPIOB->BSRR = GPIO_BSRR_BS7;
}

static inline void sda_low(void) {
    GPIOB->BSRR = GPIO_BSRR_BR7;
}

static inline uint32_t sda_read(void) {
    return (GPIOB->IDR & GPIO_IDR_IDR7) != 0u ? 1u : 0u;
}

void i2c_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    /* 先将 PB6（SCL）、PB7（SDA）输出锁存置 1（释放总线，由外部上拉拉高） */
    GPIOB->BSRR = GPIO_BSRR_BS6 | GPIO_BSRR_BS7;

    /* PB6、PB7 配置为 50 MHz 通用开漏输出（CNF=01, MODE=11 -> 0x7） */
    GPIOB->CRL = (GPIOB->CRL & ~(0xFFu << 24)) | (0x77u << 24);
}

void i2c_start(void) {
    /* 先释放 SDA、再拉高 SCL，使本函数也可在 SCL 为低时直接产生重复起始条件 */
    sda_high();
    scl_high();
    i2c_delay_half();
    sda_low();
    i2c_delay_half();
    scl_low();
    i2c_delay_half();
}

void i2c_stop(void) {
    sda_low();
    i2c_delay_half();
    scl_high();
    i2c_delay_half();
    sda_high();
    i2c_delay_half();
}

bool i2c_write_byte(uint8_t byte) {
    for (uint32_t i = 0; i < 8u; ++i) {
        if ((byte & 0x80u) != 0u) {
            sda_high();
        } else {
            sda_low();
        }
        byte <<= 1;
        i2c_delay_half();
        scl_high();
        i2c_delay_half();
        scl_low();
    }

    /* 第 9 个时钟：主机写 1 释放 SDA，由从机拉低表示 ACK */
    sda_high();
    i2c_delay_half();
    scl_high();
    i2c_delay_half();
    bool ack = (sda_read() == 0u);
    scl_low();
    i2c_delay_half();
    return ack;
}

uint8_t i2c_read_byte(bool ack) {
    uint8_t rx = 0;
    sda_high();
    for (uint32_t i = 0; i < 8u; ++i) {
        i2c_delay_half();
        scl_high();
        i2c_delay_half();
        rx = (uint8_t)((rx << 1) | sda_read());
        scl_low();
    }

    /* 第 9 个时钟：主机发送 ACK（拉低 SDA）或 NACK（释放 SDA） */
    if (ack) {
        sda_low();
    } else {
        sda_high();
    }
    i2c_delay_half();
    scl_high();
    i2c_delay_half();
    scl_low();
    i2c_delay_half();
    sda_high();
    return rx;
}

bool at24c02_write_byte(uint8_t word_addr, uint8_t data) {
    i2c_start();
    bool ok = i2c_write_byte((uint8_t)((AT24C02_ADDR << 1) | 0u)) &&
              i2c_write_byte(word_addr) &&
              i2c_write_byte(data);
    i2c_stop();
    return ok;
}

bool at24c02_wait_ready(uint32_t max_tries) {
    for (uint32_t i = 0; i < max_tries; ++i) {
        i2c_start();
        bool ack = i2c_write_byte((uint8_t)((AT24C02_ADDR << 1) | 0u));
        i2c_stop();
        if (ack) {
            return true;
        }
    }
    return false;
}

bool at24c02_read_byte(uint8_t word_addr, uint8_t *data) {
    return at24c02_read(word_addr, data, 1u);
}

bool at24c02_read(uint8_t word_addr, void *buf, size_t len) {
    if (len == 0u) {
        return true;
    }
    uint8_t *dst = (uint8_t *)buf;

    /* 1. 伪写：把从机内部地址指针拨到 word_addr */
    i2c_start();
    if (!i2c_write_byte((uint8_t)((AT24C02_ADDR << 1) | 0u)) ||
        !i2c_write_byte(word_addr)) {
        i2c_stop();
        return false;
    }

    /* 2. 重复起始条件 + 读寻址字节（R/W = 1） */
    i2c_start();
    if (!i2c_write_byte((uint8_t)((AT24C02_ADDR << 1) | 1u))) {
        i2c_stop();
        return false;
    }

    /* 3. 前 len - 1 个字节回 ACK，最后一个字节回 NACK 后发停止条件 */
    for (size_t i = 0; i < len; ++i) {
        dst[i] = i2c_read_byte(i + 1u < len);
    }
    i2c_stop();
    return true;
}
