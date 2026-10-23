# B.8 libbpf、BTF 与 CO-RE

正文见 `docs/教程/正文/系统编程/拓展B_eBPF/B_8_libbpf_BTF与CO_RE.md`。

| 文件 | 用途 |
| --- | --- |
| `task_state.bpf.c`、`task_state.c` | 兼容 `task_struct` 的两个成员名（`__state` 与旧内核的 `state`），演示 `bpf_core_field_exists` 与 `___` 后缀 |
| `three_ways.bpf.c`、`three_ways.c` | 同一个 `real_parent->tgid` 的三种读法：手写 `bpf_probe_read_kernel`、`BPF_CORE_READ`、fentry 里直接解引用 |
| `kconfig_example.bpf.c` | `__kconfig` 外部对象：`LINUX_KERNEL_VERSION` 与带 `__weak` 的 `CONFIG_BPF_JIT_ALWAYS_ON` |
| `Makefile` | 生成 `vmlinux.h`、编译目标文件、生成骨架、链接 libbpf |

`vmlinux.h` 由 Makefile 生成，不入库：

```sh
bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h
```

运行需要 root 或 `CAP_BPF`、`CAP_PERFMON`。它们要加载 BPF 程序，只应在自己的虚拟机或测试机上运行。