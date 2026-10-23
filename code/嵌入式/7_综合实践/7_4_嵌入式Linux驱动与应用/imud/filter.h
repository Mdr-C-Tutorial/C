#define _POSIX_C_SOURCE 200809L

#ifndef IMUD_FILTER_H
#define IMUD_FILTER_H

#include "imud.h"

double accel_pitch_deg(const struct imu_sample *s);
double accel_roll_deg(const struct imu_sample *s);
void attitude_init(struct attitude *a, const struct imu_sample *s);
void attitude_update(struct attitude *a, const struct imu_sample *s, double dt,
                     double alpha);

#endif
