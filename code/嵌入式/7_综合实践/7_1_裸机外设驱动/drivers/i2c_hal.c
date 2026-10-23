/*
 * i2c_hal.c：STM32F1 HAL 的 I2C1 主机后端（PB6/PB7，100 kHz）。
 *
 * HAL 的超时参数是毫秒；这里统一用 10 ms。HAL 的返回值有三种：OK、ERROR
 * （从机无应答）、TIMEOUT/BUSY，对上层都归结为 false。
 *
 * 本文件需要 stm32f1xx-hal-driver 1.1.10 与一份 stm32f1xx_hal_conf.h。
 * 2.8 已经讲过 HAL 的初始化和轮询读写，这里只做包装。
 */
#include "i2c_bus.h"

#include "stm32f1xx_hal.h"

#define I2C_HAL_TIMEOUT_MS 10u

static I2C_HandleTypeDef g_hi2c1;

static bool hal_write(void *ctx, uint8_t addr7, const uint8_t *data, size_t len) {
    (void)ctx;
    HAL_StatusTypeDef st =
        HAL_I2C_Master_Transmit(&g_hi2c1, (uint16_t)(addr7 << 1), (uint8_t *)data,
                                (uint16_t)len, I2C_HAL_TIMEOUT_MS);
    return st == HAL_OK;
}

static bool hal_write_read(void *ctx, uint8_t addr7, const uint8_t *wr, size_t wr_len,
                           uint8_t *rd, size_t rd_len) {
    (void)ctx;
    HAL_StatusTypeDef st =
        HAL_I2C_Master_Transmit(&g_hi2c1, (uint16_t)(addr7 << 1), (uint8_t *)wr,
                                (uint16_t)wr_len, I2C_HAL_TIMEOUT_MS);
    if (st != HAL_OK) {
        return false;
    }
    st = HAL_I2C_Master_Receive(&g_hi2c1, (uint16_t)((addr7 << 1) | 1u), rd,
                                (uint16_t)rd_len, I2C_HAL_TIMEOUT_MS);
    return st == HAL_OK;
}

void i2c_hal_create(i2c_bus_t *bus) {
    g_hi2c1.Instance = I2C1;
    g_hi2c1.Init.ClockSpeed = 100000u;
    g_hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    g_hi2c1.Init.OwnAddress1 = 0;
    g_hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    g_hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    g_hi2c1.Init.OwnAddress2 = 0;
    g_hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    g_hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    (void)HAL_I2C_Init(&g_hi2c1);

    bus->write = hal_write;
    bus->write_read = hal_write_read;
    bus->ctx = &g_hi2c1;
}