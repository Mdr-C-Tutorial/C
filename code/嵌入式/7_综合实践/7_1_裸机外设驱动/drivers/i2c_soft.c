/*
 * i2c_soft.c：GPIO 模拟的 I2C 主机后端（PB6 = SCL，PB7 = SDA，外部上拉）。
 *
 * 与 2.8 的 soft_i2c.c 逐位时序相同，多出两样东西：
 *   - 等待 SCL 被从机拉低时的超时，时钟延展不会把 CPU 卡死；
 *   - 总线恢复：SDA 被某个从机拉住不放时，补发 9 个 SCL 再加停止条件。
 */
#include "i2c_bus.h"
#include "stm32f1xx.h"

#define I2C_SOFT_STRETCH_TIMEOUT 10000u /* 轮询次数，约几毫秒 */

static void delay_half(void) {
    /* 72 MHz 下约 5 us，对应 100 kHz */
    for (volatile uint32_t i = 0; i < 45u; ++i) {
        __NOP();
    }
}

static inline void scl(uint32_t level) {
    GPIOB->BSRR = (level != 0u) ? GPIO_BSRR_BS6 : GPIO_BSRR_BR6;
}

static inline void sda(uint32_t level) {
    GPIOB->BSRR = (level != 0u) ? GPIO_BSRR_BS7 : GPIO_BSRR_BR7;
}

static inline uint32_t sda_read(void) {
    return (GPIOB->IDR & GPIO_IDR_IDR7) != 0u ? 1u : 0u;
}

/* 释放 SCL 后等它真正变高；返回 false 表示从机一直拉着不放 */
static bool scl_high_with_timeout(void) {
    scl(1u);
    for (uint32_t i = 0; i < I2C_SOFT_STRETCH_TIMEOUT; ++i) {
        if ((GPIOB->IDR & GPIO_IDR_IDR6) != 0u) {
            return true;
        }
    }
    return false;
}

static void start(void) {
    sda(1u);
    scl(1u);
    delay_half();
    sda(0u);
    delay_half();
    scl(0u);
    delay_half();
}

static void stop(void) {
    sda(0u);
    delay_half();
    scl(1u);
    delay_half();
    sda(1u);
    delay_half();
}

static bool write_byte(uint8_t byte) {
    for (uint32_t i = 0; i < 8u; ++i) {
        sda((byte & 0x80u) != 0u ? 1u : 0u);
        byte <<= 1;
        delay_half();
        if (!scl_high_with_timeout()) {
            return false;
        }
        delay_half();
        scl(0u);
    }
    sda(1u); /* 第 9 个时钟释放 SDA，让从机拉低表示应答 */
    delay_half();
    if (!scl_high_with_timeout()) {
        return false;
    }
    delay_half();
    bool ack = sda_read() == 0u;
    scl(0u);
    delay_half();
    return ack;
}

static uint8_t read_byte(bool ack) {
    uint8_t rx = 0u;
    sda(1u);
    for (uint32_t i = 0; i < 8u; ++i) {
        delay_half();
        if (!scl_high_with_timeout()) {
            return 0xFFu;
        }
        rx = (uint8_t)((rx << 1) | (sda_read() & 1u));
        delay_half();
        scl(0u);
    }
    sda(ack ? 0u : 1u);
    delay_half();
    (void)scl_high_with_timeout();
    delay_half();
    scl(0u);
    delay_half();
    return rx;
}

/* 9 个时钟把卡住的从机走完当前字节，再补一个停止条件 */
static void bus_recover(void) {
    sda(1u);
    for (uint32_t i = 0; i < 9u; ++i) {
        scl(1u);
        delay_half();
        scl(0u);
        delay_half();
    }
    stop();
}

static bool soft_write(void *ctx, uint8_t addr7, const uint8_t *data, size_t len) {
    (void)ctx;
    start();
    bool ok = write_byte((uint8_t)(addr7 << 1));
    for (size_t i = 0; ok && i < len; ++i) {
        ok = write_byte(data[i]);
    }
    stop();
    if (!ok) {
        bus_recover();
    }
    return ok;
}

static bool soft_write_read(void *ctx, uint8_t addr7, const uint8_t *wr,
                            size_t wr_len, uint8_t *rd, size_t rd_len) {
    (void)ctx;
    start();
    bool ok = write_byte((uint8_t)(addr7 << 1));
    for (size_t i = 0; ok && i < wr_len; ++i) {
        ok = write_byte(wr[i]);
    }
    if (ok && rd_len > 0u) {
        start(); /* 重复起始条件 */
        ok = write_byte((uint8_t)((addr7 << 1) | 1u));
        for (size_t i = 0; ok && i < rd_len; ++i) {
            rd[i] = read_byte(i + 1u < rd_len);
        }
    }
    stop();
    if (!ok) {
        bus_recover();
    }
    return ok;
}

void i2c_soft_create(i2c_bus_t *bus) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    GPIOB->BSRR = GPIO_BSRR_BS6 | GPIO_BSRR_BS7; /* 先释放总线 */
    /* PB6、PB7：50 MHz 开漏输出（CNF=01, MODE=11 -> 0x7） */
    GPIOB->CRL = (GPIOB->CRL & ~(0xFFu << 24)) | (0x77u << 24);

    bus->write = soft_write;
    bus->write_read = soft_write_read;
    bus->ctx = 0;
}