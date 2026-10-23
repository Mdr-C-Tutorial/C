# 第一个 eBPF 程序的构建文件。
#
#   make        生成 vmlinux.h，编译内核态程序，生成骨架，链接用户态程序
#   make clean  删除生成的目标文件、骨架和可执行文件
#
# 需要 clang、bpftool、libbpf 开发包和 pkg-config。

CLANG ?= clang
BPFTOOL ?= bpftool
CC ?= cc

BPF_CFLAGS := -g -O2 -target bpf -mcpu=v3 -D__TARGET_ARCH_x86 -I.
CFLAGS := -std=c17 -Wall -Wextra
LIBBPF_CFLAGS := $(shell pkg-config --cflags libbpf)
LIBBPF_LIBS := $(shell pkg-config --libs libbpf)

all: hello

# 内核的 BTF 导出成 C 头文件。只在第一次构建时生成，之后一直复用。
vmlinux.h:
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

hello.bpf.o: hello.bpf.c vmlinux.h
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

# 骨架把 hello.bpf.o 嵌进头文件，所以两者必须一起更新
hello.skel.h: hello.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

hello: hello.c hello.skel.h
	$(CC) $(CFLAGS) $(LIBBPF_CFLAGS) $< -o $@ $(LIBBPF_LIBS)

clean:
	rm -f hello hello.bpf.o hello.skel.h

.PHONY: all clean
