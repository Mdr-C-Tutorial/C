#define _POSIX_C_SOURCE 200809L

#ifndef IMUD_H
#define IMUD_H

#include <stdint.h>

/* 与驱动 uapi 头 driver/mdr_imu.h 中的结构逐字节对应 */
struct imu_sample {
    uint64_t timestamp_ns;
    int16_t accel[3]; /* mg */
    int16_t gyro[3];  /* 0.01 °/s */
    uint16_t accel_range;
    uint16_t reserved;
};

_Static_assert(sizeof(struct imu_sample) == 24, "样本必须是 24 字节");

/* 互补滤波的状态与结果 */
struct attitude {
    double pitch; /* 度 */
    double roll;  /* 度 */
};

#define IMUD_CONFIG_PATH_MAX 256

struct imud_config {
    char source[IMUD_CONFIG_PATH_MAX];
    char host[IMUD_CONFIG_PATH_MAX];
    char port[16];
    char device_id[32];
    char topic_prefix[64];
    int period_ms;
    int sample_rate;
    double alpha;
};

#endif