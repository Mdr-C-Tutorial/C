# 本页的检查用 Makefile：生成 vmlinux.h，编译并检查一个最小的 BPF 程序。
# 完整的工程 Makefile 见 B.3。

BPFTOOL ?= bpftool
CLANG ?= clang
CC ?= cc

VMLINUX_BTF ?= /sys/kernel/btf/vmlinux
CFLAGS ?= -std=c17 -Wall -Wextra
BPF_CFLAGS := -g -O2 -target bpf -mcpu=v3 -D__TARGET_ARCH_x86 -I.

all: smoke.skel.h btfstat libbpfver

# vmlinux.h 由内核的 BTF 生成，不放进仓库。
vmlinux.h: $(VMLINUX_BTF)
	$(BPFTOOL) btf dump file $(VMLINUX_BTF) format c > $@

smoke.bpf.o: smoke.bpf.c vmlinux.h
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

smoke.skel.h: smoke.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

btfstat: btfstat.c
	$(CC) $(CFLAGS) -o $@ $<

libbpfver: libbpfver.c
	$(CC) $(CFLAGS) -o $@ $< $$(pkg-config --cflags --libs libbpf)

.PHONY: clean
clean:
	rm -f vmlinux.h smoke.bpf.o smoke.skel.h btfstat libbpfver
