# 4.8 RT-Thread

本节示例的编译检查方式：只编目标文件、不链接成完整固件（msh 与设备框架需要
完整 BSP 才有意义）。

## 获取 RT-Thread 源码

第三方源码不提交仓库。在工作目录拉取 v5.3.0：

```bash
git clone --depth 1 --branch v5.3.0 https://github.com/RT-Thread/rt-thread.git
# 无法访问 GitHub 时用 gitee 镜像：
# git clone --depth 1 --branch v5.3.0 https://gitee.com/rtthread/rt-thread.git
```

## 编译检查

```bash
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -std=gnu11 -O2 -Wall -Wextra \
    -ffreestanding -D__RTTHREAD__ \
    -I<rt-thread>/include -I<rt-thread>/libcpu/arm/cortex-m3 -I. \
    -c producer_consumer_rtt.c
```

带设备框架的示例（`uart_device_sample.c`、`pin_blink.c`）再加：

```bash
    -DRT_USING_DEVICE -DRT_USING_DEVICE_IPC -DRT_USING_PIN \
    -I<rt-thread>/components/drivers/include \
    -I<rt-thread>/components/drivers/include/drivers
```

`init_export_demo.c` 需要 FinSH 头文件与符号表开关：

```bash
    -DFINSH_USING_SYMTAB -I<rt-thread>/components/finsh
```

## 编译内核并链接

`build_kernel.sh` 的做法（本节实测用）：

1. 编 `src/` 下的内核源文件（`thread.c`、`ipc.c`、`object.c`、`kservice.c`、
   `mem.c`、`timer.c`、`scheduler_up.c`、`scheduler_comm.c`、`clock.c`、
   `idle.c`、`components.c`、`irq.c`、`defunct.c`、`cpu_up.c`）与
   `src/klibc/` 下的 `kstring.c`、`kstdio.c`、`kerrno.c`、
   `rt_vsnprintf_std.c`、`rt_vsscanf.c`，外加
   `libcpu/arm/cortex-m3/cpuport.c` 与 `context_gcc.S`；
2. 打包成 `librtthread.a`；
3. 用本目录的 `link.ld`（带 `.rti_fn` 与 `FSymTab` 的收集段）与
   `../common/startup_stm32f103.c` 链接。注意启动文件要跳 `entry()`
   （RT-Thread 的入口），把 `main` 定义成 `entry` 再编译即可：
   `-Dmain=entry`。

`link.ld` 是本节为编译检查写的最小链接脚本，不是 blue-pill BSP 的
`board/linker_scripts/link.lds`；两者的关键段相同。
