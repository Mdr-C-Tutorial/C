################################################################################
#
# mdr-imu —— MPU6050 的字符设备驱动，随内核一起编进根文件系统
#
################################################################################

MDR_IMU_VERSION = 1.0
MDR_IMU_SITE = $(BR2_EXTERNAL_MDR_PATH)/../driver
MDR_IMU_SITE_METHOD = local
MDR_IMU_LICENSE = GPL-2.0
MDR_IMU_LICENSE_FILES = mdr_mpu6050.c

$(eval $(kernel-module))
$(eval $(generic-package))