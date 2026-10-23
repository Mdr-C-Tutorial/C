# B.12 网络应用：XDP 与 tc 的示例
#
#   make           编译内核态目标文件与用户态加载程序
#   make objects   只编译两个 .bpf.o，不链接用户态（Windows 上也能做）
#   make clean     删除生成的文件
#
# vmlinux.h 由本机内核的 BTF 生成，不随仓库分发。

CLANG ?= clang
CC ?= cc
BPFTOOL ?= bpftool
CFLAGS ?= -g -O2 -Wall -Wextra
BPF_CFLAGS = -g -O2 -target bpf -mcpu=v3 -D__TARGET_ARCH_x86 -I.
USER_CFLAGS = -std=c17 -Wall -Wextra -g
USER_LIBS = $(shell pkg-config --cflags --libs libbpf)

BPF_OBJS = xdp_count.bpf.o tcx_log.bpf.o
SKELS = xdp_count.skel.h tcx_log.skel.h
PROGS = xdp_count tcx_log test_run

all: $(PROGS)

objects: $(BPF_OBJS)

vmlinux.h:
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

%.bpf.o: %.bpf.c vmlinux.h
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

%.skel.h: %.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

xdp_count: xdp_count.c xdp_count.skel.h
	$(CC) $(USER_CFLAGS) $< -o $@ $(USER_LIBS)

tcx_log: tcx_log.c tcx_log.skel.h
	$(CC) $(USER_CFLAGS) $< -o $@ $(USER_LIBS)

test_run: test_run.c xdp_count.skel.h
	$(CC) $(USER_CFLAGS) $< -o $@ $(USER_LIBS)

parse_test: parse_test.c
	$(CC) $(USER_CFLAGS) $< -o $@

clean:
	rm -f $(BPF_OBJS) $(SKELS) $(PROGS) parse_test vmlinux.h

.PHONY: all objects clean