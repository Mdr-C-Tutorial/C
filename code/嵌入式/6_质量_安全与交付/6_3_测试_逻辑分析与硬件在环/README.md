# 6.3 测试、逻辑分析与硬件在环

本目录包含教程 6.3 节的配套示例。主机测试用 MinGW gcc，目标测试固件用 arm-none-eabi-gcc，HIL 脚本用 Python 3 标准库（接真串口时才需要 pyserial）。

## 主机测试

```bash
cmake -S . -B build -G Ninja
cmake --build build
cd build && ctest --output-on-failure
```

直接运行时二进制会打印每个测试的名字与统计行。

## 覆盖率

```bash
cd build
./test_mpu6050_cov
gcov test_mpu6050_cov-mpu6050.gcda
```

## 目标上的测试固件

```bash
cmake -S . -B build_arm -G Ninja --toolchain ../../common/arm-none-eabi.cmake \
    -DCMSIS_DIR=/path/to/cmsis
cmake --build build_arm
```

得到 `test_fw_target.elf`，烧进板子后从 USART1（PA9/PA10，115200）输出与主机上相同的测试结果。

## HIL 脚本

```bash
python hil/test_cli.py              # 进程内的假设备，不需要硬件
python hil/test_cli.py --port COM5  # 真串口，需要 pyserial
```

## 文件

| 文件 | 说明 |
| --- | --- |
| `common/test_assert.h` | 与 Unity v2.7.0 同名同形的最小断言集 |
| `fake_usart/uart_byte.c`、`.h` | 接收外设指针的 UART 字节驱动 |
| `fake_usart/test_uart_byte.c` | 伪造寄存器的测试 |
| `fake_i2c/mpu6050.c`、`.h` | 依赖总线接口的 MPU6050 驱动 |
| `fake_i2c/test_mpu6050.c` | 伪造总线的测试（主机与目标共用） |
| `link_replace/app_logic.c`、`fake_tick.c` | 链接替换：测试构建用假的时间源 |
| `link_replace/test_blink.c` | 闪烁逻辑的测试 |
| `target_test/target_init.c` | 目标构建的串口初始化与 `_write` |
| `hil/test_cli.py` | pytest 风格的 HIL 脚本，带假设备 |