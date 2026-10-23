#define _POSIX_C_SOURCE 200809L

/*
 * imud 的单元测试。不依赖硬件与网络：样本拼装、字节序、互补滤波、
 * 配置解析都在这里验证。
 */

#include "config.h"
#include "filter.h"
#include "imud.h"
#include "sample_io.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static int checks;

#define CHECK(cond)                                                            \
    do {                                                                       \
        checks++;                                                              \
        if (!(cond)) {                                                         \
            failures++;                                                        \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
        }                                                                      \
    } while (0)

static void check_near(double got, double want, double tol, const char *what) {
    checks++;
    if (fabs(got - want) > tol) {
        failures++;
        printf("FAIL %s: got %.4f want %.4f (tol %.4f)\n", what, got, want,
               tol);
    }
}

static void test_byte_order(void) {
    struct imu_sample s = {
        .timestamp_ns = 0x0102030405060708ull,
        .accel = { 1000, -1000, 250 },
        .gyro = { 131, -262, 0 },
        .accel_range = 2,
    };
    unsigned char raw[24];
    struct imu_sample back;

    sample_to_le_bytes(&s, raw);
    CHECK(raw[0] == 0x08);
    CHECK(raw[7] == 0x01);
    CHECK(raw[8] == 0xE8 && raw[9] == 0x03); /* 1000 = 0x03E8，小端 */
    CHECK(raw[10] == 0x18 && raw[11] == 0xFC); /* −1000 = 0xFC18 */
    back = sample_from_le_bytes(raw);
    CHECK(back.timestamp_ns == s.timestamp_ns);
    CHECK(back.accel[0] == 1000 && back.accel[1] == -1000);
    CHECK(back.accel[2] == 250);
    CHECK(back.gyro[1] == -262);
    CHECK(back.accel_range == 2);
}

static void make_sample(unsigned char raw[24], uint64_t ts, int16_t ax,
                        int16_t ay, int16_t az) {
    struct imu_sample s = {.timestamp_ns = ts,
                           .accel = {ax, ay, az},
                           .gyro = {0, 0, 0}};

    sample_to_le_bytes(&s, raw);
}

static void test_reader_partial(void) {
    struct sample_reader sr;
    struct imu_sample out[4];
    unsigned char raw[24];
    size_t got;

    sample_reader_init(&sr);
    make_sample(raw, 1000, 0, 0, 1000);

    /* 先喂 10 字节：一个样本也凑不齐 */
    got = sample_reader_feed(&sr, raw, 10, out, 4);
    CHECK(got == 0);
    CHECK(sample_reader_pending(&sr) == 10);

    /* 再喂 14 字节，正好凑齐一个 */
    got = sample_reader_feed(&sr, raw + 10, 14, out, 4);
    CHECK(got == 1);
    CHECK(out[0].accel[2] == 1000);
    CHECK(sample_reader_pending(&sr) == 0);

    /* 一次喂 2.5 个样本：取两个，剩半个 */
    make_sample(raw, 2000, 0, 0, 1000);
    got = sample_reader_feed(&sr, raw, 24, out, 4);
    CHECK(got == 1);
    got = sample_reader_feed(&sr, raw, 24, out, 4);
    CHECK(got == 1);
    got = sample_reader_feed(&sr, raw, 12, out, 4);
    CHECK(got == 0);
    CHECK(sample_reader_pending(&sr) == 12);
}

static void test_reader_capacity(void) {
    struct sample_reader sr;
    struct imu_sample out[2];
    unsigned char raw[24 * 5];
    size_t got;

    for (int i = 0; i < 5; i++) {
        make_sample(raw + 24 * i, (uint64_t)(i + 1) * 1000, 0, 0, 1000);
    }
    sample_reader_init(&sr);
    /* 输出数组只能放两个，剩下的整样本必须留在内部缓冲里 */
    got = sample_reader_feed(&sr, raw, sizeof raw, out, 2);
    CHECK(got == 2);
    CHECK(sample_reader_pending(&sr) == 3 * 24);
}

static void test_attitude_level(void) {
    struct imu_sample s = {.accel = {0, 0, 1000}, .gyro = {0, 0, 0}};
    struct attitude a;

    attitude_init(&a, &s);
    check_near(a.pitch, 0.0, 1e-6, "level pitch");
    check_near(a.roll, 0.0, 1e-6, "level roll");

    /* 平放 1 秒，陀螺仪不动，姿态不该漂 */
    for (int i = 0; i < 100; i++) {
        attitude_update(&a, &s, 0.01, 0.98);
    }
    check_near(a.pitch, 0.0, 1e-6, "still pitch");
    check_near(a.roll, 0.0, 1e-6, "still roll");
}

static void test_attitude_tilt(void) {
    /* x 轴 -577 mg、z 轴 816 mg，约 35.26° 的俯仰 */
    struct imu_sample s = {.accel = {-577, 0, 816}, .gyro = {0, 0, 0}};
    struct attitude a;

    attitude_init(&a, &s);
    check_near(a.pitch, 35.26, 0.05, "tilt pitch");
}

static void test_attitude_gyro_tracks(void) {
    /* 加速度计一直报水平，但陀螺仪以 90 °/s 绕 x 轴转：滤波结果居中 */
    struct imu_sample still = {.accel = {0, 0, 1000}, .gyro = {0, 0, 0}};
    struct imu_sample moving = {.accel = {0, 0, 1000}, .gyro = {9000, 0, 0}};
    struct attitude a;

    attitude_init(&a, &still);
    /*
     * 每步陀螺仪贡献 0.9°，再乘 0.98：p = 0.98(p + 0.9)，稳态 44.1°。
     * 时间常数是 dt/(1−alpha) = 0.5 s，300 步（3 秒）后误差降到 1% 以内。
     */
    for (int i = 0; i < 300; i++) {
        attitude_update(&a, &moving, 0.01, 0.98);
    }
    check_near(a.pitch, 44.0, 0.5, "gyro steady state");
}

static void test_config(void) {
    struct imud_config cfg;
    FILE *fp = fopen("test_imud.conf", "w");

    CHECK(fp != NULL);
    if (fp == NULL) {
        return;
    }
    fprintf(fp, "# 测试配置\n");
    fprintf(fp, "source = /tmp/sensors.fifo\n");
    fprintf(fp, "period_ms = 250\n");
    fprintf(fp, "sample_rate=200\n");
    fprintf(fp, "alpha = 0.95\n");
    fclose(fp);

    config_defaults(&cfg);
    CHECK(cfg.period_ms == 1000);
    CHECK(strcmp(cfg.source, "/dev/mdr_imu") == 0);
    CHECK(config_load("test_imud.conf", &cfg) == 0);
    CHECK(strcmp(cfg.source, "/tmp/sensors.fifo") == 0);
    CHECK(cfg.period_ms == 250);
    CHECK(cfg.sample_rate == 200);
    check_near(cfg.alpha, 0.95, 1e-9, "alpha");

    /* 非法取值必须整体拒绝，cfg 保持不变 */
    fp = fopen("test_imud.conf", "w");
    CHECK(fp != NULL);
    if (fp != NULL) {
        fprintf(fp, "period_ms = 5\n");
        fclose(fp);
    }
    CHECK(config_load("test_imud.conf", &cfg) != 0);
    CHECK(cfg.period_ms == 250);

    remove("test_imud.conf");
}

int main(void) {
    test_byte_order();
    test_reader_partial();
    test_reader_capacity();
    test_attitude_level();
    test_attitude_tilt();
    test_attitude_gyro_tracks();
    test_config();

    printf("%d 项检查，%d 项失败\n", checks, failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}