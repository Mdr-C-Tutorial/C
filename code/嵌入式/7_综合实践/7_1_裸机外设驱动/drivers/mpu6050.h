#ifndef MPU6050_H
#define MPU6050_H

#include <stdbool.h>
#include <stdint.h>

#include "i2c_bus.h"

/* 7 位地址：AD0 接低时为 0x68，接高为 0x69 */
#define MPU6050_ADDR7 0x68u

/* 常用寄存器（InvenSense Register Map） */
#define MPU6050_REG_SMPLRT_DIV   0x19u
#define MPU6050_REG_CONFIG       0x1Au
#define MPU6050_REG_GYRO_CONFIG  0x1Bu
#define MPU6050_REG_ACCEL_CONFIG 0x1Cu
#define MPU6050_REG_ACCEL_XOUT_H 0x3Bu
#define MPU6050_REG_PWR_MGMT_1   0x6Bu
#define MPU6050_REG_WHO_AM_I     0x75u

/* ±2 g 量程下 1 g 对应 16384 LSB */
#define MPU6050_LSB_PER_G 16384

typedef struct {
    int16_t ax, ay, az; /* mg */
    int16_t temp;       /* 0.01 °C */
    int16_t gx, gy, gz; /* 0.01 °/s */
} mpu6050_sample_t;

typedef struct {
    const i2c_bus_t *bus;
    uint8_t addr7;
} mpu6050_t;

/* 校验 WHO_AM_I、唤醒芯片、设置采样率与量程。返回 false 时 err 为访问失败或 ID 不符。 */
bool mpu6050_init(mpu6050_t *dev, const i2c_bus_t *bus, uint8_t addr7, uint8_t *who);

/* 一次读 14 字节，加速度、温度、陀螺仪。 */
bool mpu6050_read_all(mpu6050_t *dev, mpu6050_sample_t *out);

/* 原始值换算：LSB → mg，四舍五入到最近的整数。给主机测试单独调。 */
int16_t mpu6050_raw_to_mg(int16_t raw);

#endif /* MPU6050_H */