#!/bin/sh
# i2c-stub 冒烟测试：在 QEMU 上预置寄存器，比对 sysfs 的返回值。
# 开发板上没有 i2c-stub，不能跑这个脚本。
#
# 用法：./stub_smoke_test.sh
# 前提：内核带 i2c-stub 与 i2c-dev 模块，mdr_mpu6050.ko 已经装进
# /lib/modules/$(uname -r)/extra/，当前用户是 root。

set -eu

CHIP=0x68
SYS=/sys/class/misc/mdr_imu
FAIL=0

fail() {
    echo "FAIL: $*"
    FAIL=1
}

check() {
    want=$1
    got=$2
    what=$3

    if [ "$got" = "$want" ]; then
        echo "ok   $what = $got"
    else
        fail "$what 期望 $want，实际 $got"
    fi
}

modprobe i2c-stub chip_addr=$CHIP
modprobe mdr_mpu6050

# 预置 WHO_AM_I、PWR_MGMT_1 与六个加速度字节：
#   ax = 0x0190 = 400 → 24 mg
#   ay = 0xffae = -82 → -5 mg
#   az = 0x4000 = 16384 → 1000 mg
i2cset -y 0 $CHIP 0x75 0x68
i2cset -y 0 $CHIP 0x6b 0x00
i2cset -y 0 $CHIP 0x3b 0x01 0x90 0xff 0xae 0x40 0x00 i

if [ ! -e /sys/bus/i2c/devices/i2c-0/new_device ]; then
    fail "没有 i2c-0 的 new_device，i2c-stub 可能没加载"
    exit 1
fi
echo mdr_mpu6050 $CHIP > /sys/bus/i2c/devices/i2c-0/new_device
sleep 0.5

# 驱动注册成 misc 设备，属性在 /sys/class/misc/mdr_imu 下
check 100 "$(cat $SYS/sample_rate)" "sample_rate"
check 0 "$(cat $SYS/accel_range)" "accel_range"

# 从字符设备读一个样本：前 8 字节是时间戳，接着是 ax、ay、az
if [ ! -c /dev/mdr_imu ]; then
    fail "/dev/mdr_imu 不存在"
else
    # 用 dd 取 24 字节，od 展开成十进制 16 位有符号数
    set -- $(dd if=/dev/mdr_imu bs=24 count=1 2>/dev/null | od -An -td2 -j8 -N6)
    check 24 "$1" "样本 ax (mg)"
    check -5 "$2" "样本 ay (mg)"
    check 1000 "$3" "样本 az (mg)"
fi

# 写 sample_rate 应当落到 SMPLRT_DIV 上：200 Hz → 分频 4
echo 200 > $SYS/sample_rate
check 200 "$(cat $SYS/sample_rate)" "写入后的 sample_rate"
check 0x04 "$(i2cget -y 0 $CHIP 0x19)" "SMPLRT_DIV"

# 非法取值必须被拒绝，属性保持原值
if echo 5000 > $SYS/sample_rate 2>/dev/null; then
    fail "sample_rate 接受了 5000"
fi
check 200 "$(cat $SYS/sample_rate)" "拒绝非法值后的 sample_rate"

# 卸载设备，驱动应当让芯片回到休眠（PWR_MGMT_1 的 SLEEP 位）
echo $CHIP > /sys/bus/i2c/devices/i2c-0/delete_device
sleep 0.3
check 0x40 "$(i2cget -y 0 $CHIP 0x6b)" "卸载后的 PWR_MGMT_1"

rmmod mdr_mpu6050 2>/dev/null || true
modprobe -r i2c-stub 2>/dev/null || true

if [ "$FAIL" -eq 0 ]; then
    echo "冒烟测试通过"
    exit 0
fi
echo "冒烟测试失败"
exit 1