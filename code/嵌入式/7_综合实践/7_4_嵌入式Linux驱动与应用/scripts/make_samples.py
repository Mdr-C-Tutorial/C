#!/usr/bin/env python3
"""生成 24 字节一个的样本流，写到标准输出。

用法：make_samples.py [-n 数量] [-r 采样率] [--tilt 角度]
供 imud 集成测试用：把输出重定向进命名管道，imud 从管道读。
时间戳按 CLOCK_BOOTTIME 的纳秒值递增，驱动与这个脚本用同一个时钟。
"""

import argparse
import math
import struct
import sys
import time


def sample(ts_ns, ax, ay, az, gx=0, gy=0, gz=0, accel_range=0):
    return struct.pack("<Qhhh hhh HH", ts_ns, ax, ay, az, gx, gy, gz,
                       accel_range, 0)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("-n", "--count", type=int, default=100)
    p.add_argument("-r", "--rate", type=int, default=100)
    p.add_argument("--tilt", type=float, default=0.0, help="俯仰角，单位度")
    p.add_argument("--burst", action="store_true",
                   help="一次写完，不按采样间隔等待（联调与测试用）")
    args = p.parse_args()

    rad = args.tilt * 3.141592653589793 / 180.0
    az = int(1000 * math.cos(rad))
    ax = int(-1000 * math.sin(rad))
    period_ns = 1_000_000_000 // args.rate
    # 只需要递增的纳秒值；驱动那侧用的是 CLOCK_BOOTTIME
    base = time.monotonic_ns()
    out = sys.stdout.buffer

    for i in range(args.count):
        out.write(sample(base + i * period_ns, ax, 0, az))
        out.flush()
        if not args.burst:
            time.sleep(period_ns / 1e9)


if __name__ == "__main__":
    main()