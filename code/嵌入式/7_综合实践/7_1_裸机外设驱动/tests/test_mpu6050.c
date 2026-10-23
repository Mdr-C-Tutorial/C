/*
 * test_mpu6050.c：用伪造的 I2C 总线测试 MPU6050 驱动。
 *
 * 不接硬件、不需要 Unity：断言失败时打印文件与行号，进程返回非 0。
 * 编译：gcc -std=c17 -Wall -Wextra test_mpu6050.c ../drivers/mpu6050.c -I../drivers
 */
#include <stdio.h>
#include <string.h>

#include "i2c_bus.h"
#include "mpu6050.h"

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

static int g_failures;

/* 假总线：记录最后一次事务，按预置的寄存器内容应答 */
struct fake_bus {
    uint8_t regs[128];
    uint8_t last_write[16];
    size_t last_write_len;
    uint8_t last_reg;
    int fail_at; /* 第几次事务失败，-1 表示永不失败 */
    int transactions;
};

static bool fake_write(void *ctx, uint8_t addr7, const uint8_t *data, size_t len) {
    struct fake_bus *fb = (struct fake_bus *)ctx;
    ++fb->transactions;
    if (fb->fail_at == fb->transactions) {
        return false;
    }
    if (addr7 != MPU6050_ADDR7) {
        return false;
    }
    fb->last_write_len = len;
    memcpy(fb->last_write, data, len > sizeof fb->last_write ? sizeof fb->last_write : len);
    if (len >= 2u) {
        fb->regs[data[0]] = data[1];
    }
    return true;
}

static bool fake_write_read(void *ctx, uint8_t addr7, const uint8_t *wr, size_t wr_len,
                            uint8_t *rd, size_t rd_len) {
    struct fake_bus *fb = (struct fake_bus *)ctx;
    ++fb->transactions;
    if (fb->fail_at == fb->transactions || addr7 != MPU6050_ADDR7 || wr_len != 1u) {
        return false;
    }
    fb->last_reg = wr[0];
    for (size_t i = 0; i < rd_len; ++i) {
        rd[i] = fb->regs[(wr[0] + i) & 0x7Fu];
    }
    return true;
}

static void put_be16(struct fake_bus *fb, uint8_t reg, int16_t value) {
    fb->regs[reg] = (uint8_t)((uint16_t)value >> 8);
    fb->regs[reg + 1u] = (uint8_t)((uint16_t)value & 0xFFu);
}

int main(void) {
    struct fake_bus fb;
    i2c_bus_t bus = {fake_write, fake_write_read, &fb};

    /* 1. 正常初始化：WHO_AM_I = 0x68，之后唤醒并配置 */
    memset(&fb, 0, sizeof fb);
    fb.fail_at = -1;
    fb.regs[MPU6050_REG_WHO_AM_I] = 0x68u;

    mpu6050_t dev;
    uint8_t who = 0u;
    printf("[1] init with WHO_AM_I = 0x68\n");
    CHECK(mpu6050_init(&dev, &bus, MPU6050_ADDR7, &who));
    CHECK(who == 0x68u);
    CHECK((fb.regs[MPU6050_REG_PWR_MGMT_1] & 0x40u) == 0u); /* SLEEP 已清 */
    CHECK(fb.regs[MPU6050_REG_CONFIG] == 0x03u);
    printf("    init ok, PWR_MGMT_1 = 0x%02X, CONFIG = 0x%02X\n",
           (unsigned)fb.regs[MPU6050_REG_PWR_MGMT_1],
           (unsigned)fb.regs[MPU6050_REG_CONFIG]);

    /* 2. ID 不符：初始化失败，且不会继续写寄存器 */
    memset(&fb, 0, sizeof fb);
    fb.fail_at = -1;
    fb.regs[MPU6050_REG_WHO_AM_I] = 0x70u;
    printf("[2] init with WHO_AM_I = 0x70\n");
    CHECK(!mpu6050_init(&dev, &bus, MPU6050_ADDR7, &who));
    CHECK(who == 0x70u);
    CHECK(fb.regs[MPU6050_REG_PWR_MGMT_1] == 0u);
    printf("    rejected as expected, no write follows\n");

    /* 3. 14 字节拼接与换算：ax = 1 g，ay = -1 g，az = 0.5 g */
    memset(&fb, 0, sizeof fb);
    fb.fail_at = -1;
    fb.regs[MPU6050_REG_WHO_AM_I] = 0x68u;
    put_be16(&fb, MPU6050_REG_ACCEL_XOUT_H + 0, 16384);
    put_be16(&fb, MPU6050_REG_ACCEL_XOUT_H + 2, -16384);
    put_be16(&fb, MPU6050_REG_ACCEL_XOUT_H + 4, 8192);
    put_be16(&fb, MPU6050_REG_ACCEL_XOUT_H + 6, 0);
    put_be16(&fb, MPU6050_REG_ACCEL_XOUT_H + 8, 131);   /* 131 LSB/(°/s) -> +100（0.01 °/s） */
    put_be16(&fb, MPU6050_REG_ACCEL_XOUT_H + 10, -131); /* 同上，负方向 */
    put_be16(&fb, MPU6050_REG_ACCEL_XOUT_H + 12, 0);

    mpu6050_sample_t s;
    CHECK(mpu6050_init(&dev, &bus, MPU6050_ADDR7, NULL));
    printf("[3] read_all with one 14-byte transaction\n");
    CHECK(mpu6050_read_all(&dev, &s));
    CHECK(s.ax == 1000);
    CHECK(s.ay == -1000);
    CHECK(s.az == 500);
    CHECK(s.temp == 3653);
    CHECK(s.gx == 100);
    CHECK(s.gy == -100);
    printf("    ax=%d ay=%d az=%d mg, temp=%d, gx=%d gy=%d gz=%d\n", s.ax, s.ay,
           s.az, s.temp, s.gx, s.gy, s.gz);

    /* 4. 中途无应答：read_all 返回 false，调用方据此报错 */
    memset(&fb, 0, sizeof fb);
    fb.fail_at = -1;
    fb.regs[MPU6050_REG_WHO_AM_I] = 0x68u;
    CHECK(mpu6050_init(&dev, &bus, MPU6050_ADDR7, NULL));
    fb.fail_at = fb.transactions + 1; /* 下一次事务无应答 */
    printf("[4] read_all with NACK on first transaction\n");
    CHECK(!mpu6050_read_all(&dev, &s));
    printf("    returned false as expected\n");

    /* 5. 换算的舍入方向 */
    printf("[5] raw_to_mg rounding\n");
    CHECK(mpu6050_raw_to_mg(0) == 0);
    CHECK(mpu6050_raw_to_mg(8) == 0);   /* 0.49 mg */
    CHECK(mpu6050_raw_to_mg(9) == 1);   /* 0.55 mg */
    CHECK(mpu6050_raw_to_mg(-9) == -1);
    CHECK(mpu6050_raw_to_mg(16384) == 1000);
    printf("    8->%d 9->%d -9->%d 16384->%d\n", mpu6050_raw_to_mg(8),
           mpu6050_raw_to_mg(9), mpu6050_raw_to_mg(-9), mpu6050_raw_to_mg(16384));

    if (g_failures == 0) {
        printf("\nall tests passed\n");
        return 0;
    }
    printf("\n%d failure(s)\n", g_failures);
    return 1;
}