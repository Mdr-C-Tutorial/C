# 4.1 临界区与 RTOS 边界

本目录是教程 4.1 节的配套代码，分成两类。

## Cortex-M3 片段

`race_disasm.c`、`basepri_lock.c`、`basepri_nest.c`、`atomic_inc.c`、`cp64.c`、`mpsc_reserve.c` 只做编译与反汇编，编译命令写在各自文件开头，需要一个放有 CMSIS 头文件的目录（CMSIS_5 5.9.0 的 `CMSIS/Core/Include` 与 cmsis_device_f1 v4.3.5 的 `Include` 放在一起）。例如：

```bash
arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra \
    -DSTM32F103xB -I<cmsis 目录> -c race_disasm.c
arm-none-eabi-objdump -d race_disasm.o
```

`superloop_vs_tasks/rtos_tasks.c` 是同一需求的 FreeRTOS 版本，只做编译检查，实际运行在 4.2 节的目标工程里。

## PC 程序

| 目标 | 文件 | 内容 |
| --- | --- | --- |
| `superloop` | `superloop_vs_tasks/superloop.c` | 用假时钟跑完整的非阻塞超级循环 |
| `mpsc_test` | `test_mpsc.c`、`mpsc_reserve.c` | 两个线程各写一万条，验证多生产者单消费者缓冲区 |
| `mpsc_ringbuf_starter` | `exercises/mpsc_ringbuf_starter.c` | 同题目的习题起始代码，`mpsc_rb_put()` 与 `mpsc_rb_get()` 待补全 |
| `clock64_starter` | `exercises/clock64_starter.c` | 64 位时钟读取的习题起始代码 |

```bash
cmake -B build -G Ninja
cmake --build build
./build/superloop
./build/mpsc_test
```