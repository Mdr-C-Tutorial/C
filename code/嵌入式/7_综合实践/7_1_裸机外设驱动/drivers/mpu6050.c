/*
 * mpu6050.c：MPU6050 六轴传感器驱动。
 *
 * 只依赖 i2c_bus_t，不包含任何 stm32f1xx.h。主机测试里换一个记录事务、
 * 预置返回数据的假总线，就能在不接硬件的情况下把初始化与拼接逻辑走一遍。
 */
#include "mpu6050.h"

#include <string.h>

/* 上电后芯片处于睡眠状态，清掉 SLEEP 位并选用 PLL 作为时钟源 */
#define PWR_MGMT_1_WAKE 0x01u

/* DLPF 约 44 Hz（CONFIG = 3），加速度输出率 1 kHz */
#define CONFIG_DLPF_44HZ 0x03u

static int16_t be16(const uint8_t *p) {
    return (int16_t)((uint16_t)((uint16_t)p[0] << 8) | p[1]);
}

/* raw / 16384 得到 g，乘 1000 得到 mg；整数四舍五入 */
int16_t mpu6050_raw_to_mg(int16_t raw) {
    int32_t scaled = (int32_t)raw * 1000;
    if (scaled >= 0) {
        return (int16_t)((scaled + MPU6050_LSB_PER_G / 2) / MPU6050_LSB_PER_G);
    }
    return (int16_t)((scaled - MPU6050_LSB_PER_G / 2) / MPU6050_LSB_PER_G);
}

bool mpu6050_init(mpu6050_t *dev, const i2c_bus_t *bus, uint8_t addr7, uint8_t *who) {
    dev->bus = bus;
    dev->addr7 = addr7;

    uint8_t id = 0u;
    if (!i2c_read_reg(bus, addr7, MPU6050_REG_WHO_AM_I, &id)) {
        return false;
    }
    if (who != NULL) {
        *who = id;
    }
    if (id != MPU6050_ADDR7) {
        return false;
    }

    /* 唤醒，时钟源取 X 轴陀螺 PLL；写 0x00 则会用内部 8 MHz 振荡器 */
    if (!i2c_write_reg(bus, addr7, MPU6050_REG_PWR_MGMT_1, PWR_MGMT_1_WAKE)) {
        return false;
    }
    /* 采样率 = 1 kHz / (1 + SMPLRT_DIV)，这里取 1 kHz */
    if (!i2c_write_reg(bus, addr7, MPU6050_REG_SMPLRT_DIV, 0u)) {
        return false;
    }
    if (!i2c_write_reg(bus, addr7, MPU6050_REG_CONFIG, CONFIG_DLPF_44HZ)) {
        return false;
    }
    /* 量程保持复位值 ±2 g、±250 °/s，无需写 ACCEL_CONFIG/GYRO_CONFIG */
    return true;
}

bool mpu6050_read_all(mpu6050_t *dev, mpu6050_sample_t *out) {
    uint8_t reg = MPU6050_REG_ACCEL_XOUT_H;
    uint8_t buf[14];

    if (!dev->bus->write_read(dev->bus->ctx, dev->addr7, &reg, 1u, buf, sizeof buf)) {
        return false;
    }

    out->ax = mpu6050_raw_to_mg(be16(&buf[0]));
    out->ay = mpu6050_raw_to_mg(be16(&buf[2]));
    out->az = mpu6050_raw_to_mg(be16(&buf[4]));
    /* 温度：raw / 340 + 36.53 °C，这里用 0.01 °C 的整数表示 */
    int16_t raw_temp = be16(&buf[6]);
    out->temp = (int16_t)((int32_t)raw_temp * 100 / 340 + 3653);
    out->gx = (int16_t)((int32_t)be16(&buf[8]) * 100 / 131);
    out->gy = (int16_t)((int32_t)be16(&buf[10]) * 100 / 131);
    out->gz = (int16_t)((int32_t)be16(&buf[12]) * 100 / 131);
    return true;
}