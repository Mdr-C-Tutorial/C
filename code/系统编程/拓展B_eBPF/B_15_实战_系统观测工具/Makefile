# openwatch：监视 openat 的 eBPF 工具
#
#   make          生成 vmlinux.h、编译内核态程序、生成骨架、链接用户态程序
#   make clean    删除生成的文件

CLANG ?= clang
BPFTOOL ?= bpftool
CC ?= cc

.PHONY: all clean
.DELETE_ON_ERROR:

all: openwatch

# 目标机器的内核 BTF 转成 C 头文件；不入库
vmlinux.h:
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

openwatch.bpf.o: openwatch.bpf.c openwatch.h vmlinux.h
	$(CLANG) -g -O2 -target bpf -mcpu=v3 -D__TARGET_ARCH_x86 -I. -c $< -o $@

# 骨架把目标文件嵌进 C 数组，生成 openwatch_bpf__open 等函数
openwatch.skel.h: openwatch.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

openwatch: openwatch.c openwatch.h openwatch.skel.h
	$(CC) -std=c17 -Wall -Wextra -g openwatch.c -o $@ $$(pkg-config --cflags --libs libbpf)

clean:
	rm -f openwatch openwatch.bpf.o openwatch.skel.h vmlinux.h
