# 4.2 任务、调度与时间

本目录是教程 4.2 节的配套示例，也建立了第 4 章后续各页共用的 FreeRTOS 构建骨架（`../common/freertos/`）。

## 获取 FreeRTOS-Kernel

第三方源码不进仓库，构建前先拉一份 V11.3.0：

```bash
git clone --depth 1 --branch V11.3.0 https://github.com/FreeRTOS/FreeRTOS-Kernel.git
```

## 模拟器示例（PC）

| 文件 | 内容 |
| --- | --- |
| `two_tasks.c` | 两个任务交替打印，最小可运行程序 |
| `task_param.c` | 同一任务函数以不同参数创建两个实例 |
| `starvation.c` | 高优先级忙等与阻塞两种写法（`BUSY_WAIT` 切换） |
| `delay_drift.c` | `vTaskDelay` 与 `xTaskDelayUntil` 的唤醒时刻对比 |
| `idle_hook.c` | 空闲任务与 `vApplicationIdleHook()` |

Windows 上用 Ninja 或 MinGW Makefiles：

```bash
cmake -B build -G Ninja -DFREERTOS_KERNEL_PATH=<FreeRTOS-Kernel 目录>
cmake --build build
./build/two_tasks
```

Linux 与 macOS 上换用默认的 Unix Makefiles，`freertos.cmake` 会自动改用 POSIX 移植层并链接 `pthread`。

## STM32F103 目标

`blinky_rtos/` 是闪 PC13 的 LED 并每秒打印一次的两个任务。交叉编译需要 CMSIS 头文件（CMSIS_5 5.9.0 的 `CMSIS/Core/Include` 与 cmsis_device_f1 v4.3.5 的 `Include`，两处内容放进同一个目录）：

```bash
cmake -B build_stm32 -G Ninja \
    --toolchain ../../common/arm-none-eabi.cmake \
    -DFREERTOS_KERNEL_PATH=<FreeRTOS-Kernel 目录> \
    -DCMSIS_INCLUDE_DIR=<CMSIS 头文件目录>
cmake --build build_stm32
```

## 共用的构建骨架

`../common/freertos/` 下的 `freertos.cmake` 提供 `emb_freertos_add_kernel()`：按构建目标选移植层与配置模板，把配置和控制台源文件复制到构建目录，再把 FreeRTOS-Kernel 的 CMake 工程加进来。应用侧只需要：

```cmake
include("${CMAKE_CURRENT_LIST_DIR}/../../common/freertos/freertos.cmake")
emb_freertos_add_kernel()
target_link_libraries(<目标> PRIVATE freertos_kernel freertos_config)
```

控制台的两种实现（模拟器的 `vprintf` 与目标上的 USART1）也在这个目录里，由 `freertos.cmake` 复制到 `EMB_FREERTOS_SUPPORT_DIR` 后编译。
