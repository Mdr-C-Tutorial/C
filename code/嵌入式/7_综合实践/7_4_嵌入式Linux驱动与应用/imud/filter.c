#define _POSIX_C_SOURCE 200809L

#include "filter.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* 加速度计的原始值里重力约 1000 mg，换成弧度用的比例是 g/1000 */
static double accel_to_g(int16_t mg) { return (double)mg / 1000.0; }

double accel_roll_deg(const struct imu_sample *s) {
    double ay = accel_to_g(s->accel[1]);
    double az = accel_to_g(s->accel[2]);

    return atan2(ay, az) * 180.0 / M_PI;
}

double accel_pitch_deg(const struct imu_sample *s) {
    double ax = accel_to_g(s->accel[0]);
    double ay = accel_to_g(s->accel[1]);
    double az = accel_to_g(s->accel[2]);
    double norm = sqrt(ax * ax + ay * ay + az * az);

    if (norm < 1e-9) {
        return 0.0;
    }
    return atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / M_PI;
}

void attitude_init(struct attitude *a, const struct imu_sample *s) {
    a->pitch = accel_pitch_deg(s);
    a->roll = accel_roll_deg(s);
}

void attitude_update(struct attitude *a, const struct imu_sample *s, double dt,
                     double alpha) {
    /* 陀螺仪单位是 0.01 °/s，先换成 °/s */
    double gx = s->gyro[0] / 100.0;
    double gy = s->gyro[1] / 100.0;

    double pitch_acc = accel_pitch_deg(s);
    double roll_acc = accel_roll_deg(s);

    /* 陀螺仪积分预测，再用加速度计的低频分量修正 */
    a->pitch = alpha * (a->pitch + gx * dt) + (1.0 - alpha) * pitch_acc;
    a->roll = alpha * (a->roll + gy * dt) + (1.0 - alpha) * roll_acc;
}