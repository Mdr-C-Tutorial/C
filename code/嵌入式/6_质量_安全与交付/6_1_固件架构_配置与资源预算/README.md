# 6.1 固件架构、配置与资源预算

本目录包含教程 6.1 节的配套示例。全部输出都来自本机 arm-none-eabi-gcc 与 Python 3，不需要硬件。

## 获取 CMSIS 头文件

示例只用到 CMSIS-Core 与 cmsis_device_f1 的头文件，不提交进仓库：

```bash
git clone --depth 1 --branch v4.3.5 https://github.com/STMicroelectronics/cmsis_device_f1.git
git clone --depth 1 --branch v5.9.0 https://github.com/ARM-software/CMSIS_5.git
```

把两个仓库整理成下面的布局，作为 `-DCMSIS_DIR` 传进去：

```text
cmsis/
├── CMSIS/Core/Include/          # CMSIS_5 里复制过来
└── cmsis_device_f1/Include/     # cmsis_device_f1 里复制过来
```

## 构建

```bash
cmake -S . -B build -G Ninja --toolchain ../../common/arm-none-eabi.cmake \
    -DCMSIS_DIR=/path/to/cmsis -DBOARD=bluepill
cmake --build build
```

`-DBOARD` 取 `bluepill` 或 `minimal`，对应 `config/` 下的两个板级头文件。

构建结束后会看到 `size_demo.elf` 与 `size_demo_float.elf` 的 `size` 输出，以及预算检查的结果。把 `budget/budget.json` 里的 `flash` 改小，构建会以非零状态失败。

## 文件

| 文件 | 说明 |
| --- | --- |
| `size_demo/main.c` | LED + UART printf 的最小固件，体积对比的基线 |
| `size_demo/retarget.c` | `_write` 接到 USART1 |
| `config/board.h` | 板级配置入口，按 `BOARD_HEADER` 引入具体板子 |
| `config/board_bluepill.h`、`config/board_minimal.h` | 两块板子的引脚与能力宏 |
| `config/app_config.h` | 功能开关与缓冲区大小，带 `#error` 与 `_Static_assert` 检查 |
| `config/config_check.c` | 只做配置检查的编译单元 |
| `stack/demo.c` | 四种典型栈帧，配合 `-fstack-usage` |
| `budget/check_budget.py` | 读 ELF 的 size 输出，对照 `budget.json` |
| `budget/budget.json` | 预算上限（字节） |

## 单独量栈

```bash
arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -fstack-usage -c stack/demo.c
arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -fcallgraph-info=su -c stack/demo.c
```

分别得到 `demo.su` 与 `demo.ci`。