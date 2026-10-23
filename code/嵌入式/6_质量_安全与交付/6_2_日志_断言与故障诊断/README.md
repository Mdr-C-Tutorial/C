# 6.2 日志、断言与故障诊断

本目录包含教程 6.2 节的配套示例。主机程序用 MinGW gcc 编译运行，目标代码用 arm-none-eabi-gcc 编译检查，不需要硬件。

## 主机示例

```bash
cmake -S . -B build -G Ninja
cmake --build build

./build/log_demo          # 默认 LOG_LEVEL=INF
./build/assert_demo       # 正常路径
./build/assert_demo bad   # 故意触发断言，退出码 2
./build/test_diag         # 诊断记录的单元测试
./build/test_store        # 用模拟 Flash 测试记录存储
./build/diagdump          # 打印一条填充好的诊断记录
```

日志的级别用编译期宏改：

```bash
gcc -std=c17 -Wall -Wextra -DLOG_LEVEL=4 -Ilog log/log.c log/log_net.c log/log_demo.c -o log_dbg
```

## 目标侧编译检查

```bash
cmake -S . -B build_arm -G Ninja --toolchain ../../common/arm-none-eabi.cmake
cmake --build build_arm
```

会编出 `log_fw.elf`（日志的体积对比）与目标侧的 `diag_target` 对象库。只改 `-DLOG_LEVEL` 重新构建，可以复现正文的体积表。

## 栈扫描

```bash
# 先用 arm-none-eabi-gcc 编译 crashfw/fw.c 得到 fw.elf（命令见 fw.c 注释）
python crashfw/mkstack.py            # 在 crashfw/ 下生成 stack.bin
./build/stackscan crashfw/fw.elf crashfw/stack.bin
arm-none-eabi-addr2line -e crashfw/fw.elf -f -a 0x0800013F 0x08000153 0x08000167 0x080001A3
```

`stack.bin` 的输入是构造的，`stackscan` 与 `addr2line` 的输出是真实的。

## 文件

| 文件 | 说明 |
| --- | --- |
| `log/log.h`、`log/log.c` | 分级日志，级别在编译期过滤 |
| `log/log_demo.c`、`log/log_net.c` | 两个模块的日志演示 |
| `log/log_fw.c` | 目标侧的日志体积对比 |
| `assert/mdr_assert.h`、`assert/assert_demo.c` | 记录返回地址的断言宏与主机演示 |
| `diag/diag_record.c`、`diag/diag_record.h` | 诊断记录、面包屑与 CRC |
| `diag/diag_store.c`、`diag/diag_store.h` | 记录追加到外部 Flash 的槽式存储 |
| `diag/test_diag.c`、`diag/test_store.c` | 两份主机测试 |
| `diag/diagdump.c` | 打印一条完整的诊断记录 |
| `stackscan/stackscan.c` | 从栈转储里筛候选返回地址 |
| `crashfw/fw.c`、`crashfw/mkstack.py` | 演示调用链的固件与构造栈转储的脚本 |