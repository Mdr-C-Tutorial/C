# 4.5 内存与资源管理

本目录包含教程 4.5 节的配套示例。RAM 布局与栈用量的输出需要 arm-none-eabi 工具链，其余示例在 PC 上运行。

## 获取 FreeRTOS-Kernel 源码

第三方源码不提交仓库，构建前先在工作目录拉取 `FreeRTOS-Kernel` V11.3.0：

```bash
git clone --depth 1 --branch V11.3.0 https://github.com/FreeRTOS/FreeRTOS-Kernel.git
```

## 构建并运行（PC 模拟器）

```bash
cmake -B build_sim -G Ninja -DFREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel
cmake --build build_sim
./build_sim/heap_frag_sim
./build_sim/static_tasks
./build_sim/pool_sim
./build_sim/test_mem_pool
```

`test_mem_pool` 不链接 FreeRTOS，是 `mem_pool` 的主机单元测试。

本页示例需要打开 `configSUPPORT_STATIC_ALLOCATION` 与栈溢出检测，公共的
`common/freertos/FreeRTOSConfig_sim.h` 里这两项是关的，所以本目录自带一份
`FreeRTOSConfig_mem.h`，构建时复制成构建目录里的 `FreeRTOSConfig.h`。

## 构建目标固件（STM32F103C8）

```bash
cmake -B build_stm32 -G Ninja \
    --toolchain ../../common/arm-none-eabi.cmake \
    -DCMSIS_INCLUDE_DIR=/path/to/cmsis \
    -DFREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel
cmake --build build_stm32
```

`-DCMSIS_INCLUDE_DIR` 指向同时放有 CMSIS_5 5.9.0 的 `CMSIS/Core/Include` 与
cmsis_device_f1 v4.3.5 的 `Include` 的目录。

构建结束后用 `arm-none-eabi-size -A` 与 `arm-none-eabi-nm --size-sort` 观察 `build_stm32/ram_map.elf` 的 RAM 分布。栈用量的 `.su` 文件由 `stack_usage` 目标在构建目录中生成：
`build_stm32/CMakeFiles/stack_usage.dir/stack_usage.c.su`。

## 文件

| 文件 | 说明 |
| --- | --- |
| `ram_map.c` | 静态与动态任务混合的固件，用于观察 RAM 布局 |
| `static_tasks.c` | 全静态创建任务与队列；`-DUSE_DYNAMIC_ALLOC=1` 切换成动态版 |
| `main_stack_bug.c` | `main()` 局部对象地址交给任务的错误写法；`-DSHOW_BUG` 切换 |
| `stack_usage.c` | 配合 `-fstack-usage` 观察各函数栈帧 |
| `heap_frag_sim.c` | heap_4 的碎片演示 |
| `mem_pool.c` / `mem_pool.h` | 固定块内存池 |
| `pool_sim.c` | 两个任务通过队列传递内存池缓冲区 |
| `test_mem_pool.c` | 内存池的主机单元测试 |
| `FreeRTOSConfig_mem.h` | 本页专用的 FreeRTOS 配置（打开静态分配与栈溢出检测） |