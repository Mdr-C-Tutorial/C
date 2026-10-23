# 4.4 中断与任务协作

本目录包含教程 4.4 节的配套示例：中断里用 FromISR API 把事件交给任务，以及把异步 DMA 驱动包装成阻塞接口。

## 获取 FreeRTOS-Kernel 源码

第三方源码不提交仓库，构建前先在工作目录拉取 `FreeRTOS-Kernel` V11.3.0：

```bash
git clone --depth 1 --branch V11.3.0 https://github.com/FreeRTOS/FreeRTOS-Kernel.git
```

## 构建并运行（PC 模拟器）

```bash
cmake -B build_sim -G Ninja -DFREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel
cmake --build build_sim
./build_sim/isr_notify_sim
./build_sim/isr_notify_noyield
./build_sim/uart_cli_sim        # 只在 Windows 上构建
```

`isr_notify_sim` 与 `isr_notify_noyield` 是同一个程序的两种编译：后者丢掉了 `higher_woken`，用来对比被唤醒任务的运行时刻。`uart_cli_sim` 用一个 Windows 线程模拟串口外设。

## 构建目标固件（STM32F103C8）

```bash
cmake -B build_stm32 -G Ninja \
    --toolchain ../../common/arm-none-eabi.cmake \
    -DFREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel
cmake --build build_stm32
```

四个目标分别演示串口接收、串口发送、ADC 的 DMA 通知与中断优先级设置。`isr_priority_wrong` 是故意不设优先级的反例，只用来对照阅读。

## 文件

| 文件 | 说明 |
| --- | --- |
| `isr_notify_sim.c` | 模拟 ADC 的 DMA 完成中断 → 任务通知 → 处理任务；`-DNO_YIELD_FROM_ISR` 为反例 |
| `uart_cli_sim.c` | Windows 线程模拟串口 → 流缓冲区 → 命令行任务 |
| `uart_rx_rtos.c` | USART1 空闲中断 + DMA1 通道 5 → 流缓冲区 → 命令行任务（目标构建） |
| `uart_tx_blocking.c` | DMA 发送 + 完成中断中的任务通知 + 互斥量，组成阻塞的 `uart_write()`（目标构建） |
| `adc_dma_notify.c` | ADC1 的 DMA 传输完成 → 任务通知 → 处理任务（目标构建） |
| `isr_priority.c` | 中断优先级与 FromISR 的正确/错误初始化；`-DWRONG_INIT` 为反例（目标构建） |