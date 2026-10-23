# 7.1 裸机外设驱动

本目录是教程 7.1 节的配套工程：STM32F103C8（Blue Pill）上的裸机命令行与 MPU6050 驱动。

## 目录

```text
7_1_裸机外设驱动/
├── app/          # main.c 调度，cli.c 命令表
├── board/        # 引脚、USART1 中断接收、SysTick、I2C 总线实例
├── drivers/      # i2c_bus.h 接口，i2c_soft.c / i2c_hal.c 两个后端，mpu6050.c
├── tests/        # 主机测试：伪造总线测驱动，分词测命令行
└── CMakeLists.txt
```

依赖方向：`app/` 可以包含 `board/` 与 `drivers/`，`board/` 可以包含 `drivers/`，反之不行。`drivers/mpu6050.c` 只包含 `i2c_bus.h`，可以在主机上编译。

## 主机测试（不需要硬件）

```bash
cmake -B build_host -G Ninja
cmake --build build_host
./build_host/test_mpu6050
./build_host/test_cli
```

也可以不用 CMake，直接编译：

```bash
gcc -std=c17 -Wall -Wextra -O2 tests/test_mpu6050.c drivers/mpu6050.c -Idrivers -o test_mpu6050
gcc -std=c17 -Wall -Wextra -O2 tests/test_cli.c app/cli.c -Iapp -o test_cli
```

## 目标固件

需要 Arm GNU Toolchain 与 CMSIS 头文件（`core_cm3.h`、`stm32f1xx.h`、`stm32f103xb.h`）。common/ 目录由上层提供（见 `code/嵌入式/common/`）。

```bash
cmake -B build -G Ninja --toolchain ../../common/arm-none-eabi.cmake \
      -DARM_TOOLCHAIN_DIR=<工具链的 bin 目录> \
      -DCMSIS_INCLUDE_DIR=<CMSIS 头文件目录>
cmake --build build
```

产物是 `build/mdr_bare.elf`，同时生成 `.bin` 与 `.hex`。`firmware.cmake` 的 `stm32f103_firmware()` 会在构建结束后打印 `size`。

## HAL 后端

`drivers/i2c_hal.c` 需要 STM32F1 HAL 1.1.10 与一份 `stm32f1xx_hal_conf.h`。默认的 CMakeLists 不编它，因为它要额外链接 HAL 源文件；需要时把 `drivers/i2c_hal.c` 加到目标源文件里，并把 `board_init()` 里的 `i2c_soft_create(&g_i2c_bus)` 换成 `i2c_hal_create(&g_i2c_bus)`。接线与地址不变。

## 硬件

| 信号 | 引脚 |
| --- | --- |
| USART1 TX / RX | PA9 / PA10 |
| I²C SCL / SDA | PB6 / PB7 |
| MPU6050 INT | PA8（本页不用） |
| LED | PC13（低电平亮） |
| SWD | PA13 / PA14 |

MPU6050 的 7 位地址由 AD0 决定：接低 `0x68`，接高 `0x69`。