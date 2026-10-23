# B.9 验证器：示例程序

每一对是同一个程序的错误版本与正确版本。错误版本只用 clang 编译时不会报任何错，加载时才被验证器拒绝。本目录不加载任何程序。

| 文件 | 说明 |
| --- | --- |
| `map_lookup_unchecked.bpf.c` / `map_lookup_checked.bpf.c` | map 查找结果不判空，报 `R0 invalid mem access 'map_value_or_null'` |
| `stack_padding.bpf.c` / `stack_zeroed.bpf.c` | 结构体填充字节未初始化就交给 `bpf_map_update_elem`（无特权加载时被拒） |
| `ringbuf_leak.bpf.c` / `ringbuf_released.bpf.c` | reserve 之后有一条路径没有 submit 或 discard，报 `Unreleased reference` |
| `loop_unbounded.bpf.c` / `loop_bounded.bpf.c` | 循环上界来自运行期的值，改为掩码收窄到 6 位 |
| `packet_unchecked.bpf.c` / `packet_checked.bpf.c` | 访问报文前没有与 `data_end` 比较，报 `invalid access to packet` |
| `license_mit.bpf.c` / `license_dual.bpf.c` | 许可证不兼容却调用 GPL 专用辅助函数 |
| `helper_type_xdp.bpf.c` / `helper_type_tp.bpf.c` | XDP 程序调用只有跟踪类才允许的 `bpf_get_current_comm` |
| `verify_log.c` | 用户态加载程序，把 libbpf 的日志原样转出 |
| `padding_demo.c` | 普通 C 程序，打印结构体的填充字节位置与内容 |

## 编译

内核态程序（需要 clang 的 BPF 后端；本教程用 Zig 自带的 clang）：

```bash
clang -g -O2 -target bpf -mcpu=v3 -D__TARGET_ARCH_x86 -I. -c map_lookup_unchecked.bpf.c -o map_lookup_unchecked.bpf.o
```

`vmlinux.h` 由 `bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h` 生成，不入库。

用户态程序（需要 libbpf 开发包）：

```bash
cc -std=c17 -Wall -Wextra verify_log.c -o verify_log $(pkg-config --cflags --libs libbpf)
```

`verify_log` 会真正加载程序，需要 root。`padding_demo` 是普通 C 程序，不需要任何特殊权限：

```bash
cc -std=c17 -Wall -Wextra padding_demo.c -o padding_demo
./padding_demo
```
