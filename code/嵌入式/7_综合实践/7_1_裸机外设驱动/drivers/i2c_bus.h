#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * I2C 总线接口。设备驱动只依赖这张表，不关心底下是 GPIO 模拟还是硬件外设，
 * 也不关心引脚号。ctx 由具体后端解释。
 *
 * write：先生成起始条件，发送 addr7<<1，写 data，生成停止条件。
 * write_read：先写 wr（常见的是寄存器地址），再发重复起始条件读 rd。
 * 两个函数返回 false 表示从机没有应答、总线超时或总线恢复失败。
 */
typedef struct i2c_bus {
    bool (*write)(void *ctx, uint8_t addr7, const uint8_t *data, size_t len);
    bool (*write_read)(void *ctx, uint8_t addr7, const uint8_t *wr, size_t wr_len,
                       uint8_t *rd, size_t rd_len);
    void *ctx;
} i2c_bus_t;

/* 读一个 8 位寄存器；寄存器地址本身也走一次写。驱动里最常见的操作。 */
static inline bool i2c_read_reg(const i2c_bus_t *bus, uint8_t addr7, uint8_t reg,
                                uint8_t *value) {
    return bus->write_read(bus->ctx, addr7, &reg, 1u, value, 1u);
}

/* 写一个 8 位寄存器。 */
static inline bool i2c_write_reg(const i2c_bus_t *bus, uint8_t addr7, uint8_t reg,
                                 uint8_t value) {
    uint8_t buf[2] = {reg, value};
    return bus->write(bus->ctx, addr7, buf, sizeof buf);
}

/* 两个后端的构造函数：填充函数指针与上下文，后端的实现分别在 i2c_soft.c、i2c_hal.c 里。 */
void i2c_soft_create(i2c_bus_t *bus);
void i2c_hal_create(i2c_bus_t *bus);

#endif /* I2C_BUS_H */