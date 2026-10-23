# B.5 辅助函数与上下文

正文见 `docs/教程/正文/系统编程/拓展B_eBPF/B_5_辅助函数与上下文.md`。

| 文件 | 用途 |
| --- | --- |
| `execlog.bpf.c` | `tp/sched/sched_process_exec`，取身份、父进程 tgid、comm 与 `__data_loc_filename` 指向的路径 |
| `execlog.c` | 用户态加载程序，open → load → attach → destroy，Ctrl-C 退出 |
| `id_demo.c` | 用户态演示 `getpid()` 与 `gettid()` 的关系，对应正文第 2 节的 tgid/pid 说明 |

构建（内核态、骨架、用户态）：

```sh
clang -g -O2 -target bpf -mcpu=v3 -I. -c execlog.bpf.c -o execlog.bpf.o
bpftool gen skeleton execlog.bpf.o > execlog.skel.h
cc -std=c17 -Wall -Wextra execlog.c -o execlog $(pkg-config --cflags --libs libbpf)
```

`execlog.bpf.c` 没有用 `BPF_KPROBE` 或 `BPF_PROG`，参数直接从 tracepoint 的 ctx 结构体上取，所以不需要 `-D__TARGET_ARCH_x86`。

`vmlinux.h` 由 Makefile 或手工生成，不入库：

```sh
bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h
```

`id_demo.c` 用到 pthread 与 `syscall(SYS_gettid)`，编译时加 `-pthread`，运行输出见正文第 2 节：

```sh
gcc -std=c17 -Wall -Wextra id_demo.c -o id_demo -pthread
```