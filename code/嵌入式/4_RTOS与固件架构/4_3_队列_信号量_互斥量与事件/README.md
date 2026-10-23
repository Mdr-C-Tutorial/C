# 4.3 队列、信号量、互斥量与事件

本目录是教程 4.3 节的配套示例与习题起始代码。

## 获取 FreeRTOS-Kernel

第三方源码不进仓库，构建前先在工作目录拉取 `FreeRTOS-Kernel` V11.3.0：

```bash
git clone --depth 1 --branch V11.3.0 https://github.com/FreeRTOS/FreeRTOS-Kernel.git
```

## 构建与运行（PC 模拟器）

```bash
cmake -B build_sim -G Ninja -DFREERTOS_KERNEL_PATH=<FreeRTOS-Kernel 目录>
cmake --build build_sim
./build_sim/producer_consumer
```

可执行目标：

| 目标 | 内容 |
| --- | --- |
| `producer_consumer` | 采样任务经队列把 `sample_t` 交给处理任务 |
| `mailbox` | 长度 1 的队列 + `xQueueOverwrite` / `xQueuePeek` 保存最新配置 |
| `button_semaphore` | 二值信号量把“按键按下”事件传给处理任务 |
| `console_mutex` | 不加保护与用互斥量保护控制台的两段输出对比 |
| `console_gatekeeper` | 守门任务独占控制台，其他任务投递日志消息 |
| `startup_events` | 事件组等待三个子系统全部就绪 |
| `notify_counter` | 任务通知当作计数信号量 |

## 构建（STM32F103C8 目标）

```bash
cmake -B build_stm32 -G Ninja \
    --toolchain ../../common/arm-none-eabi.cmake \
    -DFREERTOS_KERNEL_PATH=<FreeRTOS-Kernel 目录> \
    -DCMSIS_INCLUDE_DIR=<CMSIS 头文件目录>
cmake --build build_stm32
```

控制台在模拟器上是临界区里的 `vprintf`，在目标上是 USART1（115200 8N1），两种实现都在 `../../common/freertos/`，由 `freertos.cmake` 复制到构建目录。
