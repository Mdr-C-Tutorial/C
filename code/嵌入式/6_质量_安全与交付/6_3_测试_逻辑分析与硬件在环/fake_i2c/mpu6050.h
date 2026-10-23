/*
 * mpu6050.h：MPU6050 的加速度计读取，依赖 I²C 总线接口而不是具体引脚。
 *
 * 总线接口是一个函数指针表，固件里填寄存器实现，测试里填伪造实现。
 * 7.1 会给出完整的裸机驱动，这一页只用「初始化检查 + 读加速度」两件事。
 *
 * 讲解见教程 6.3 节「测试、逻辑分析与硬件在环」。
 */
#ifndef MPU6050_H
#define MPU6050_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MPU6050_ADDR 0x68u
#define MPU6050_REG_WHO_AM_I 0x75u
#define MPU6050_REG_PWR_MGMT_1 0x6Bu
#define MPU6050_REG_ACCEL_XOUT_H 0x3Bu
#define MPU6050_WHO_AM_I_VALUE 0x68u

/* ±2 g 量程下 1 g = 16384 LSB */
#define MPU6050_LSB_PER_G 16384

struct i2c_bus {
    /* 写：向 addr 的 reg 开始写 len 字节；成功返回 0 */
    int (*write_reg)(void *ctx, uint8_t addr, uint8_t reg, const uint8_t *data,
                     size_t len);
    /* 读：从 addr 的 reg 开始读 len 字节；成功返回 0 */
    int (*read_reg)(void *ctx, uint8_t addr, uint8_t reg, uint8_t *data,
                    size_t len);
    void *ctx;
};

struct mpu6050 {
    struct i2c_bus bus;
};

struct mpu6050_sample {
    int16_t ax;
    int16_t ay;
    int16_t az;
};

/* 读 WHO_AM_I 确认接线；不匹配返回 false */
bool mpu6050_init(struct mpu6050 *dev, const struct i2c_bus *bus);

/* 读三个轴的原始值；总线出错返回 false */
bool mpu6050_read_raw(struct mpu6050 *dev, struct mpu6050_sample *out);

/* 原始值换算成以 mg 为单位的整数 */
void mpu6050_to_mg(const struct mpu6050_sample *raw, int32_t out_mg[3]);

#endif /* MPU6050_H */