# 4.9 RTOS 固件架构、跟踪与测试

本目录的示例分四组：

- `ao/`：活动对象框架（`ao.c`/`ao.h`、事件定义 `event.h`）、LED 状态机
  （`led_fsm.c`/`led_fsm.h`，不含 RTOS 调用）与把状态机装进活动对象的
  `led_ao.c`，入口是 `main_ao.c`。
- `supervisor/`：任务打卡 + 监视任务 + 用打印代替的看门狗。
- `trace/`：`trace_buf.c`（跟踪钩子写 RAM 缓冲区）与
  `runtime_stats_sim.c`（`vTaskGetRunTimeStats` 输出 CPU 占比）。
- `unit_test/`：LED 状态机的主机单元测试，不需要 FreeRTOS。

## 获取 FreeRTOS-Kernel 源码

```bash
git clone --depth 1 --branch V11.3.0 https://github.com/FreeRTOS/FreeRTOS-Kernel.git
```

## 构建与运行（PC 模拟器）

```bash
cmake -B build_sim -G Ninja -DFREERTOS_KERNEL_PATH=<FreeRTOS-Kernel 目录>
cmake --build build_sim
./build_sim/ao_demo.exe
./build_sim/supervisor_demo.exe
./build_sim/trace_demo.exe
./build_sim/runtime_stats_demo.exe
./build_sim/test_led_fsm.exe
```

## 目标构建（编译检查）

```bash
cmake -B build_stm32 -G Ninja \
    --toolchain ../../common/arm-none-eabi.cmake \
    -DFREERTOS_KERNEL_PATH=<FreeRTOS-Kernel 目录> \
    -DCMSIS_INCLUDE_DIR=<CMSIS 头文件目录>
cmake --build build_stm32
```

目标分支只编两个对象库：`common/target_hooks.c`（钩子）与
`target/trace_dwt.c`（DWT 周期计数器做跟踪时间戳）。本页的示例都需要硬件
（真实任务、真实中断、真实功耗）才能观察行为，目标构建只验证代码能编过。

## 配置

`FreeRTOSConfig.h` 在公共模板基础上打开了 `configUSE_TRACE_FACILITY`、
`configUSE_STATS_FORMATTING_FUNCTIONS`、`configGENERATE_RUN_TIME_STATS` 与
`configSTATS_BUFFER_MAX_LENGTH`。模拟器用 Windows 高精度性能计数器作运行时
统计的时钟源（`common/sim_hooks.c`），目标上关掉运行时统计。
