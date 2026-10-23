/*
 * test_mpu6050.c：用伪造的 I²C 总线测试 MPU6050 驱动。
 *
 * 伪造总线记录每一次事务，并能按寄存器预置返回值、按需返回错误。
 *
 * 编译（在 fake_i2c/ 下）：
 *   gcc -std=c17 -Wall -Wextra -I. -I../common mpu6050.c test_mpu6050.c -o test_mpu6050
 *
 * 讲解见教程 6.3 节「测试、逻辑分析与硬件在环」。
 */
#include <stdio.h>
#include <string.h>

#include "../common/test_assert.h"
#include "mpu6050.h"

#define MAX_TXN 16

struct fake_i2c {
    /* 记录每一次读/写，方便断言驱动访问了什么 */
    uint8_t txn_addr[MAX_TXN];
    uint8_t txn_reg[MAX_TXN];
    uint8_t txn_len[MAX_TXN];
    int txn_count;

    uint8_t who_am_i;
    uint8_t accel[6];
    int fail_read_reg;  /* 等于这个寄存器号时返错，-1 表示不返错 */
    int fail_write_reg;
};

static struct fake_i2c g_bus;

static void fake_reset(void) {
    memset(&g_bus, 0, sizeof g_bus);
    g_bus.who_am_i = MPU6050_WHO_AM_I_VALUE;
    g_bus.fail_read_reg = -1;
    g_bus.fail_write_reg = -1;
}

static int fake_write_reg(void *ctx, uint8_t addr, uint8_t reg,
                          const uint8_t *data, size_t len) {
    struct fake_i2c *bus = ctx;
    if (bus->txn_count < MAX_TXN) {
        bus->txn_addr[bus->txn_count] = addr;
        bus->txn_reg[bus->txn_count] = reg;
        bus->txn_len[bus->txn_count] = (uint8_t)len;
    }
    bus->txn_count++;
    if (bus->fail_write_reg == (int)reg) {
        return -1;
    }
    (void)data;
    return 0;
}

static int fake_read_reg(void *ctx, uint8_t addr, uint8_t reg, uint8_t *data,
                         size_t len) {
    struct fake_i2c *bus = ctx;
    if (bus->txn_count < MAX_TXN) {
        bus->txn_addr[bus->txn_count] = addr;
        bus->txn_reg[bus->txn_count] = reg;
        bus->txn_len[bus->txn_count] = (uint8_t)len;
    }
    bus->txn_count++;
    if (bus->fail_read_reg == (int)reg) {
        return -1;
    }
    if (reg == MPU6050_REG_WHO_AM_I) {
        data[0] = bus->who_am_i;
        return 0;
    }
    if (reg == MPU6050_REG_ACCEL_XOUT_H) {
        memcpy(data, bus->accel, 6u);
        return 0;
    }
    memset(data, 0, len);
    return 0;
}

static struct i2c_bus make_bus(void) {
    struct i2c_bus bus = {
        .write_reg = fake_write_reg,
        .read_reg = fake_read_reg,
        .ctx = &g_bus,
    };
    return bus;
}

void setUp(void) {
    fake_reset();
}

void tearDown(void) {
}

static void test_init_ok(void) {
    struct mpu6050 dev;
    struct i2c_bus bus = make_bus();

    TEST_ASSERT_TRUE(mpu6050_init(&dev, &bus));
    /* 第一次事务必须是读 WHO_AM_I，地址 0x68 */
    TEST_ASSERT_EQUAL_HEX8(MPU6050_ADDR, g_bus.txn_addr[0]);
    TEST_ASSERT_EQUAL_HEX8(MPU6050_REG_WHO_AM_I, g_bus.txn_reg[0]);
    /* 第二次是把 PWR_MGMT_1 写 0 */
    TEST_ASSERT_EQUAL_HEX8(MPU6050_REG_PWR_MGMT_1, g_bus.txn_reg[1]);
}

static void test_init_wrong_device(void) {
    struct mpu6050 dev;
    struct i2c_bus bus = make_bus();

    g_bus.who_am_i = 0x70u; /* 接错芯片 */
    TEST_ASSERT_FALSE(mpu6050_init(&dev, &bus));
    /* 型号不对就不该继续写寄存器 */
    TEST_ASSERT_EQUAL_INT(1, g_bus.txn_count);
}

static void test_init_bus_nack(void) {
    struct mpu6050 dev;
    struct i2c_bus bus = make_bus();

    g_bus.fail_read_reg = MPU6050_REG_WHO_AM_I;
    TEST_ASSERT_FALSE(mpu6050_init(&dev, &bus));
}

static void test_read_raw_positive(void) {
    struct mpu6050 dev;
    struct i2c_bus bus = make_bus();
    struct mpu6050_sample s;

    TEST_ASSERT_TRUE(mpu6050_init(&dev, &bus));
    /* az = 16384，相当于 1 g */
    g_bus.accel[4] = 0x40;
    g_bus.accel[5] = 0x00;
    TEST_ASSERT_TRUE(mpu6050_read_raw(&dev, &s));
    TEST_ASSERT_EQUAL_INT(0, s.ax);
    TEST_ASSERT_EQUAL_INT(16384, s.az);
}

static void test_read_raw_negative(void) {
    struct mpu6050 dev;
    struct i2c_bus bus = make_bus();
    struct mpu6050_sample s;

    TEST_ASSERT_TRUE(mpu6050_init(&dev, &bus));
    /* ax = -16384，补码 0xC000 */
    g_bus.accel[0] = 0xC0;
    g_bus.accel[1] = 0x00;
    TEST_ASSERT_TRUE(mpu6050_read_raw(&dev, &s));
    TEST_ASSERT_EQUAL_INT(-16384, s.ax);
}

static void test_read_bus_error(void) {
    struct mpu6050 dev;
    struct i2c_bus bus = make_bus();
    struct mpu6050_sample s;

    TEST_ASSERT_TRUE(mpu6050_init(&dev, &bus));
    /* 设备在配置完成后掉线：总线上返回错误，驱动不能卡住，要返回 false */
    g_bus.fail_read_reg = MPU6050_REG_ACCEL_XOUT_H;
    TEST_ASSERT_FALSE(mpu6050_read_raw(&dev, &s));
}

static void test_to_mg(void) {
    struct mpu6050_sample raw = {.ax = 0, .ay = 8192, .az = -16384};
    int32_t mg[3];

    mpu6050_to_mg(&raw, mg);
    TEST_ASSERT_EQUAL_INT(0, mg[0]);
    TEST_ASSERT_EQUAL_INT(500, mg[1]);
    TEST_ASSERT_EQUAL_INT(-1000, mg[2]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_ok);
    RUN_TEST(test_init_wrong_device);
    RUN_TEST(test_init_bus_nack);
    RUN_TEST(test_read_raw_positive);
    RUN_TEST(test_read_raw_negative);
    RUN_TEST(test_read_bus_error);
    RUN_TEST(test_to_mg);
    return UNITY_END();
}