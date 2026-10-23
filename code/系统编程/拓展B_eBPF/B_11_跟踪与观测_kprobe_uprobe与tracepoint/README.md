# B.11 跟踪与观测：kprobe、uprobe 与 tracepoint

正文见 `docs/教程/正文/系统编程/拓展B_eBPF/B_11_跟踪与观测_kprobe_uprobe与tracepoint.md`。

| 文件 | 用途 |
| --- | --- |
| `tp_openat.bpf.c`、`tp_openat.c` | `tp/syscalls/sys_enter_openat`，读 `args[1]` 指向的路径 |
| `unlink_kprobe.bpf.c`、`unlink_kprobe.c` | `kprobe/do_unlinkat` 与 `kretprobe/do_unlinkat`，`BPF_CORE_READ` 读 `name->name` |
| `unlink_fentry.bpf.c`、`unlink_fentry.c` | 同一函数的 `fentry`/`fexit` 版本，参数可直接解引用 |
| `calc.c` | 被观测程序，两个 `noinline` 函数供 uprobe 附加 |
| `calc_uprobe.bpf.c`、`calc_uprobe.c` | 两种附加方式：SEC 名写全路径，或用 `bpf_program__attach_uprobe_opts` |
| `usdt_setjmp.bpf.c`、`usdt_setjmp.c` | 监听 `libc:setjmp` 的 USDT 探针，自动附加与手动附加各一个 |
| `perf_sample.h`、`perf_sample.bpf.c`、`perf_sample.c` | `perf_event` 按频率采样，经环形缓冲区把 `bpf_get_stack` 采到的内核栈送到用户态 |

构建（内核态、骨架、用户态）：

```sh
clang -g -O2 -target bpf -mcpu=v3 -D__TARGET_ARCH_x86 -I. -c x.bpf.c -o x.bpf.o
bpftool gen skeleton x.bpf.o > x.skel.h
cc -std=c17 -Wall -Wextra x.c -o x $(pkg-config --cflags --libs libbpf)
```

`unlink_fentry.bpf.c` 不需要 `-D__TARGET_ARCH_x86`，因为参数来自 BTF 而不是寄存器布局。`perf_sample.c` 用 `_GNU_SOURCE` 而不是 `_POSIX_C_SOURCE`，因为 `perf_event_open` 要通过 `syscall()` 调用，而 `syscall` 的声明只在 `_GNU_SOURCE` 下可见。

`usdt_setjmp.bpf.c` 包含 `<bpf/usdt.bpf.h>`，后者需要 `<linux/errno.h>` 里的三个宏。没有 Linux 系统头文件时，可以在临时目录放一个只定义 `ENOENT`、`ESRCH`、`EINVAL` 的 `linux/errno.h`，用 `-I` 指到它；这个替身头文件不进入仓库。

`vmlinux.h` 由 Makefile 生成，不入库：

```sh
bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h
```

运行这些程序需要 root 或相应的 capability（`CAP_BPF`、`CAP_PERFMON`）。它们要加载 BPF 程序，只应在自己的虚拟机或测试机上运行。