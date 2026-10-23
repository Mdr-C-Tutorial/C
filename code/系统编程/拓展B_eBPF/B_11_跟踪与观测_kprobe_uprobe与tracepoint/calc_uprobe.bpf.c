#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* 这两个程序只写 "uprobe"/"uretprobe"，libbpf 无法自动附加，
 * 由用户态程序用 bpf_program__attach_uprobe_opts 指定路径与函数名。 */
SEC("uprobe")
int BPF_UPROBE(uprobe_add, int a, int b)
{
    bpf_printk("uprobed_add ENTRY: a = %d, b = %d", a, b);
    return 0;
}

SEC("uretprobe")
int BPF_URETPROBE(uretprobe_add, int ret)
{
    bpf_printk("uprobed_add EXIT: return = %d", ret);
    return 0;
}

/* 节名里写全路径与函数名，骨架的 __attach 会自动附加。
 * 路径中带斜杠，libbpf 不再去 PATH 里查找，直接交给内核按当前目录解析。 */
SEC("uprobe/./calc:uprobed_sub")
int BPF_UPROBE(uprobe_sub, int a, int b)
{
    bpf_printk("uprobed_sub ENTRY: a = %d, b = %d", a, b);
    return 0;
}

SEC("uretprobe/./calc:uprobed_sub")
int BPF_URETPROBE(uretprobe_sub, int ret)
{
    bpf_printk("uprobed_sub EXIT: return = %d", ret);
    return 0;
}