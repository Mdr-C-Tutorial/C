# 4.6 优先级反转、死锁与实时性

本目录包含教程 4.6 节的配套示例。反转、死锁与截止时间监视在 PC 模拟器上运行，响应时间分析是普通的主机程序。

## 获取 FreeRTOS-Kernel 源码

第三方源码不提交仓库，构建前先在工作目录拉取 `FreeRTOS-Kernel` V11.3.0：

```bash
git clone --depth 1 --branch V11.3.0 https://github.com/FreeRTOS/FreeRTOS-Kernel.git
```

## 构建并运行（PC 模拟器）

```bash
cmake -B build_sim -G Ninja -DFREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel
cmake --build build_sim
./build_sim/inversion             # 二值信号量，出现无界反转
./build_sim/inversion_mutex       # 互斥量，优先级继承生效
./build_sim/deadlock_timeout      # 带超时，能看出双方都在等
./build_sim/deadlock_fixed        # 固定加锁顺序，死锁消失
./build_sim/deadline_monitor
./build_sim/rta                   # 内置三任务例子
```

默认的 `deadlock` 目标会永久停住，只在需要观察死锁现象时用 `timeout` 运行。

## 响应时间分析

`rta` 也可以从文件读任务集，每行 `C T B`（单位 ms），按周期从短到长排：

```bash
printf '2 5 1\n2 8 1\n3 20 1\n' > tasks.txt
./build_sim/rta tasks.txt
```

不带参数时用内置的 (1, 4)、(2, 6)、(3, 12) 三任务例子。

## 文件

| 文件 | 说明 |
| --- | --- |
| `inversion.c` | L/M/H 三任务的反转演示；`-DUSE_MUTEX=1` 切换成互斥量 |
| `deadlock.c` | 相反顺序加锁；`-DWITH_TIMEOUT` 加超时，`-DFIXED_ORDER` 固定顺序 |
| `rta.c` | 利用率检查与响应时间分析迭代 |
| `deadline_monitor.c` | 周期任务检测自身超期并计数 |