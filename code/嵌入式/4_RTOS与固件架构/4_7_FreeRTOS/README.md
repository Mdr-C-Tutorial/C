# 4.7 FreeRTOS

本节示例分两种构建：PC 模拟器（Windows 的 MSVC-MingW 移植层，或 Linux/macOS
的 Posix 移植层）与 STM32F103C8 目标。两者共用本目录下的一份
`FreeRTOSConfig.h`，靠 `__ARM_ARCH` 区分芯片相关的几项。

## 获取 FreeRTOS-Kernel 源码

第三方源码不提交仓库，构建前先拉取 V11.3.0：

```bash
git clone --depth 1 --branch V11.3.0 https://github.com/FreeRTOS/FreeRTOS-Kernel.git
```

## 模拟器

```bash
cmake -B build_sim -G Ninja -DFREERTOS_KERNEL_PATH=<FreeRTOS-Kernel 目录>
cmake --build build_sim
./build_sim/sw_timer_sim.exe
./build_sim/task_list_sim.exe
./build_sim/hooks_sim.exe
```

三个程序：

- `sw_timer_sim.c`：心跳 LED 与按键去抖两个软件定时器。
- `task_list_sim.c`：`vTaskList` 打印任务表。
- `hooks_sim.c`：空闲、节拍、malloc 失败、栈溢出四个钩子。

## STM32F103C8 目标

```bash
cmake -B build_stm32 -G Ninja \
    --toolchain ../../common/arm-none-eabi.cmake \
    -DFREERTOS_KERNEL_PATH=<FreeRTOS-Kernel 目录> \
    -DCMSIS_INCLUDE_DIR=<CMSIS Core 与 cmsis_device_f1 的头文件目录>
cmake --build build_stm32
```

目标上有两个可执行目标：

- `stm32_freertos.elf`：两个任务，一个翻转 PC13，一个经 USART1 打印节拍数。
- `tickless_stop`（对象库）：`tickless_stop.c` 覆盖移植层的
  `vPortSuppressTicksAndSleep()`，用 RTC 闹钟进 Stop 模式，只做编译检查，
  没有对应的可执行文件。

`CMSIS_INCLUDE_DIR` 要能让 `#include "stm32f1xx.h"` 找到文件：把 CMSIS_5 的
`CMSIS/Core/Include` 与 cmsis_device_f1 的 `Include` 两个目录的内容放在一起
即可，做法与 4.2 节相同。

## 与公共模板的关系

`../../common/freertos/` 下有第 4 章其他页面共用的 `freertos.cmake` 与两份
配置模板，它按构建目标自动挑移植层。本节为了把「一份配置同时供模拟器与目标」
演示清楚，使用本目录下的 `freertos.cmake` 与单一 `FreeRTOSConfig.h`。
