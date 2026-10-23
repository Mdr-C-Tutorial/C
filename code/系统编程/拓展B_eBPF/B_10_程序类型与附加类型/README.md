# B.10 程序类型与附加类型：配套代码

正文第 3.4 节的示例。`many_types.bpf.c` 在同一个目标文件里放了七种程序；`many_types_bad_sec.bpf.c` 是 SEC 名称拼错的版本，用来说明 `bpftool gen skeleton` 不校验 SEC 名称。

## 编译

内核态目标文件用 clang 编译，命令与 B.3 相同：

```bash
clang -g -O2 -target bpf -mcpu=v3 -D__TARGET_ARCH_x86 -I. -c many_types.bpf.c -o many_types.bpf.o
```

`vmlinux.h` 由 `bpftool btf dump file /sys/kernel/btf/vmlinux format c` 生成，不随仓库提供。MSYS2 或 Windows 上可以用 Zig 自带的 clang：

```bash
python -m ziglang cc -target bpfel-freestanding-none -mcpu=v3 -O2 -g -D__TARGET_ARCH_x86 -I <bpfinc> -c many_types.bpf.c -o many_types.bpf.o
```

## 骨架

```bash
bpftool gen skeleton many_types.bpf.o > many_types.skel.h
bpftool -d gen skeleton many_types_bad_sec.bpf.o > /dev/null   # 看 libbpf 的调试日志
```

| 文件 | 说明 |
| --- | --- |
| `many_types.bpf.c` | 七个程序：tracepoint、kprobe、fentry、xdp、tcx、cgroup_skb、lsm |
| `many_types_bad_sec.bpf.c` | 同一个 XDP 程序，SEC 名称写成 `xpd` |

两个文件都只做编译和骨架生成，不加载、不运行。正文里的 `readelf`、`bpftool gen skeleton` 与 `bpftool -d gen skeleton` 输出取自 Debian 13（bpftool 7.5、libbpf 1.5）。
