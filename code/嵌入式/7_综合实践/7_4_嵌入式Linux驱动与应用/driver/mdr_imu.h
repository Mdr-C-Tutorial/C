/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_MDR_IMU_H
#define _UAPI_MDR_IMU_H

#include <linux/types.h>

/*
 * /dev/mdr_imu 一次 read 交出的样本。定长 24 字节，用户空间按同样的
 * 布局解析；结构体里有 8 字节成员，前后两个布局的对齐都是 8。
 *
 * 时间戳取自 CLOCK_BOOTTIME，单位纳秒。用户空间用同一个时钟算间隔，
 * 不依赖驱动的采样率是否准确。
 */
struct mdr_imu_sample {
	__u64 timestamp_ns;
	__s16 accel[3];		/* x、y、z，单位 mg */
	__s16 gyro[3];		/* x、y、z，单位 0.01 °/s */
	__u16 accel_range;	/* 0～3：±2、±4、±8、±16 g */
	__u16 reserved;
};

#endif /* _UAPI_MDR_IMU_H */