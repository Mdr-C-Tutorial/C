# 7.2 RTOS 多任务设备

本目录是教程 7.2 节的配套工程：基于 FreeRTOS 的姿态记录仪，两个构建目标 `stm32f103` 与 `sim`。

## 获取第三方源码

FreeRTOS-Kernel 与 littlefs 都不提交仓库，构建前拉取：

```bash
git clone --depth 1 --branch V11.3.0 https://github.com/FreeRTOS/FreeRTOS-Kernel.git
git clone --depth 1 --branch v2.11.3 https://github.com/littlefs-project/littlefs.git
```

## 模拟器（Windows / Linux，不需要硬件）

```bash
cmake -B build_sim -G Ninja \
      -DFREERTOS_KERNEL_PATH=<FreeRTOS-Kernel 目录> \
      -DLITTLEFS_PATH=<littlefs 目录>
cmake --build build_sim
./build_sim/recorder_sim demo_script.txt
```

`demo_script.txt` 每行是“等待毫秒 命令”，调度器启动前一次读进内存，脚本跑完自动退出。脚本总时长控制在 3 秒左右：Windows 移植层的节拍线程在更长的运行里会偶发不同步，示例脚本因此写得短。不给参数时读标准输入，手敲命令后回车：`status`、`dump`、`erase`、`rate <Hz>`、`hang`、`quit`。

| 目标 | 内容 |
| --- | --- |
| `recorder_sim` | 完整记录仪：查表传感器 + lfs_rambd 上的日志 |
| `test_log` | littlefs 的写入、读回与重新挂载（不链接 FreeRTOS） |

## 目标板

```bash
cmake -B build_stm -G Ninja --toolchain ../../common/arm-none-eabi.cmake \
      -DARM_TOOLCHAIN_DIR=<工具链 bin> -DCMSIS_INCLUDE_DIR=<CMSIS 头文件> \
      -DFREERTOS_KERNEL_PATH=<FreeRTOS-Kernel 目录> \
      -DLITTLEFS_PATH=<littlefs 目录>
cmake --build build_stm
```

两份 FreeRTOS 配置在构建时分别复制到 `target_config/` 与 `sim_config/`，作为 `FreeRTOSConfig.h` 使用：目标板的堆是 10 KB、带中断优先级设置，模拟器的堆是 48 KB。

## 文件

```text
app/
├── recorder.h      # sample_t、优先级、队列长度、任务入口
├── tasks.c         # 五个任务、队列、监视打卡
├── log.c / log.h   # littlefs 上的轮转 CSV
└── console_task.c  # 命令表与处理
sim_main.c          # 模拟器板级
stm32_board.c       # 目标板级：MPU6050、W25Q64、USART1、IWDG
tests/test_log.c    # littlefs 主机测试
tests/sim_lfs.c     # 测试用的 RAM 块设备
```

W25Q64 上 littlefs 只占从 `0x030000` 起的 256 KB，前面 192 KB 留给 Bootloader 的暂存槽、备份槽与诊断区（见教程的跨页约定）。