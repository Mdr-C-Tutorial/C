# BPF 代码的 CI 检查清单

不需要 root 就能做的项目（本目录的 check.sh 就做这几件事）：

1. 编译每个内核态程序：`clang -g -O2 -target bpf -mcpu=v3 -D__TARGET_ARCH_x86 -I. -c x.bpf.c`，警告当作错误。
2. 为每个目标文件生成骨架：`bpftool gen skeleton x.bpf.o > x.skel.h`。
3. 用 `bpftool gen object` 把要合并的目标文件链接一遍，暴露重复符号与许可证不一致。
4. 用户态程序做语法检查或正常编译：`cc -std=c17 -Wall -Wextra -fsyntax-only x.c`。

需要 root、虚拟机或特殊内核才能做的项目：

- 用 veristat 加载并验证程序，记录 `Insns` 与 `States` 两列，提交前后对比，发现复杂度回退。
- 用 `BPF_PROG_TEST_RUN` 向 `BPF_PROG_TYPE_SCHED_CLS`、`BPF_PROG_TYPE_XDP` 这类程序喂输入数据。
- 在 QEMU 虚拟机里（virtme-ng、vmtest）换几个内核版本跑一次加载测试，确认 CO-RE 重定位在旧内核上仍然有效。
