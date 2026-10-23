# B.16 eBPF for Windows 与跨平台前景：配套代码

正文里每个有输出的程序都在这里。

## 文件

| 文件 | 正文位置 | 说明 |
| --- | --- | --- |
| `connect_deny.h` | 5.1 | 两个平台共用的策略常量，不包含任何平台头 |
| `connect_deny.bpf.c` | 5.1、5.2 | 一份源码，用 `MDR_PLATFORM_WINDOWS` 切换 ctx 类型与判定值 |
| `helper_id.c` | 3.2 | 同一句辅助函数调用在两套头文件下生成的 `call` 编号 |
| `ctx_layout.c` | 3.3 | 打印 Linux 与 Windows 两个 `bpf_sock_addr` 的字段偏移 |
| `verdict.c` | 5.1 | 打印 `ebpf_sock_addr_verdict_t` 三个枚举值 |

## 依赖

需要两套头文件：

- Linux 侧：`vmlinux.h`（由 `bpftool btf dump file /sys/kernel/btf/vmlinux format c` 生成）和 libbpf 的 `bpf/` 头文件；
- Windows 侧：eBPF for Windows 仓库 `include` 目录下的 `bpf_helpers.h`、`bpf_helper_defs.h`、`bpf_endian.h`、`ebpf_structs.h`、`ebpf_nethooks.h`、`ebpf_windows.h`、`bpf_helpers_platform.h`、`include/bpf/` 下的 libbpf 头文件。也可以从 NuGet 包 `eBPF-for-Windows` 的 `build\native\include` 取。

## 编译

```text
# Linux 内核态程序
clang -target bpf -Wall -Wextra -Werror -g -O2 -I. -c connect_deny.bpf.c -o connect_deny.bpf.o

# Windows 内核态程序
clang -target bpf -Wall -Wextra -Werror -O2 -g -DMDR_PLATFORM_WINDOWS ^
      -I<ebpf-for-windows>\include -I. -c connect_deny.bpf.c -o connect_deny_win.bpf.o

# helper_id：两边各编一次
clang -target bpf -Wall -Wextra -Werror -O2 -c helper_id.c -o helper_linux.o
clang -target bpf -Wall -Wextra -Werror -O2 -DMDR_PLATFORM_WINDOWS ^
      -I<ebpf-for-windows>\include -c helper_id.c -o helper_windows.o

# 两个用户态小程序（只需 Windows 头文件）
gcc -std=c17 -Wall -Wextra -I<ebpf-for-windows>\include ctx_layout.c -o ctx_layout.exe
gcc -std=c17 -Wall -Wextra -I<ebpf-for-windows>\include verdict.c -o verdict.exe
```

`connect_deny.bpf.c` 里许可证写成 `char LICENSE[] SEC("license")`，符号名大写。写成小写的 `license` 时 clang 会报 `symbol 'license' is already defined`——节名与符号名相同，链接器那一层冲突。B.10 的 3.5 节讲过这一点。

`connect_deny.bpf.o` 交给 SignalX 上的 `bpftool gen skeleton` 可以生成骨架，但用户态加载程序不在本页范围内（B.13 有完整的例子）。

## 本页代码的验证范围

内核态程序都做到了编译与反汇编检查。加载、运行需要 root、签名驱动和一台开着测试签名的 Windows 虚拟机，不在本页验证。`ctx_layout.exe` 与 `verdict.exe` 是本机的普通 C 程序，输出是真实运行结果。