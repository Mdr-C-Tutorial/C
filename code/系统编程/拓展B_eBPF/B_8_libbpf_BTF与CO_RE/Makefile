# B.8 的示例。vmlinux.h 与骨架都在构建时生成，不入库。
#
#   make            全部构建
#   make task_state 只构建兼容 __state 的那个
#   make clean
#
# 需要 clang（带 BPF 后端）、bpftool、libbpf（pkg-config 能找到）。

CLANG ?= clang
CC ?= cc
BPFTOOL ?= bpftool

BPF_CFLAGS = -g -O2 -target bpf -mcpu=v3 -D__TARGET_ARCH_x86 -I. \
             -Wall -Wextra -Wno-unused-parameter
CFLAGS = -std=c17 -Wall -Wextra -g
LIBBPF_CFLAGS = $(shell pkg-config --cflags libbpf)
LIBBPF_LIBS = $(shell pkg-config --libs libbpf)

BPF_OBJS = task_state.bpf.o three_ways.bpf.o kconfig_example.bpf.o
SKELS = task_state.skel.h three_ways.skel.h

all: task_state three_ways

vmlinux.h:
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

%.bpf.o: %.bpf.c vmlinux.h
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

%.skel.h: %.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

task_state: task_state.c task_state.skel.h
	$(CC) $(CFLAGS) $(LIBBPF_CFLAGS) $< -o $@ $(LIBBPF_LIBS)

three_ways: three_ways.c three_ways.skel.h
	$(CC) $(CFLAGS) $(LIBBPF_CFLAGS) $< -o $@ $(LIBBPF_LIBS)

clean:
	rm -f task_state three_ways
	rm -f $(BPF_OBJS) $(SKELS) vmlinux.h

.PHONY: all clean
