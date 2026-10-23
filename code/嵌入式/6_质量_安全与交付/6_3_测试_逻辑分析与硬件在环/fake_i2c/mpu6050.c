/*
 * mpu6050.c：只用总线接口，不碰任何寄存器地址。
 *
 * 讲解见教程 6.3 节「测试、逻辑分析与硬件在环」。
 */
#include "mpu6050.h"

bool mpu6050_init(struct mpu6050 *dev, const struct i2c_bus *bus) {
    uint8_t who = 0;
    if (bus == NULL || bus->read_reg == NULL) {
        return false;
    }
    dev->bus = *bus;

    if (dev->bus.read_reg(dev->bus.ctx, MPU6050_ADDR, MPU6050_REG_WHO_AM_I, &who,
                          1u) != 0) {
        return false;
    }
    if (who != MPU6050_WHO_AM_I_VALUE) {
        return false;
    }

    /* 退出睡眠：PWR_MGMT_1 写 0 */
    uint8_t wake = 0x00u;
    return dev->bus.write_reg(dev->bus.ctx, MPU6050_ADDR, MPU6050_REG_PWR_MGMT_1,
                              &wake, 1u) == 0;
}

bool mpu6050_read_raw(struct mpu6050 *dev, struct mpu6050_sample *out) {
    uint8_t buf[6];
    if (dev->bus.read_reg(dev->bus.ctx, MPU6050_ADDR, MPU6050_REG_ACCEL_XOUT_H,
                          buf, sizeof buf) != 0) {
        return false;
    }
    /* 高字节在前，两个字节是有符号的补码 */
    out->ax = (int16_t)((uint16_t)buf[0] << 8 | buf[1]);
    out->ay = (int16_t)((uint16_t)buf[2] << 8 | buf[3]);
    out->az = (int16_t)((uint16_t)buf[4] << 8 | buf[5]);
    return true;
}

void mpu6050_to_mg(const struct mpu6050_sample *raw, int32_t out_mg[3]) {
    out_mg[0] = (int32_t)raw->ax * 1000 / MPU6050_LSB_PER_G;
    out_mg[1] = (int32_t)raw->ay * 1000 / MPU6050_LSB_PER_G;
    out_mg[2] = (int32_t)raw->az * 1000 / MPU6050_LSB_PER_G;
}