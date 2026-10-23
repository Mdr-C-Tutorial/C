# openstat：按进程名统计 openat 次数，与 bpftrace 的 @[comm] = count() 等价。
# syscallstat：同样的统计换成 raw_syscalls，用来演示 bpftool gen object 链接。
# vmlinux.h 不入库，从当前内核的 BTF 生成。

CLANG ?= clang
CC ?= cc
CFLAGS := -g -O2 -target bpf -mcpu=v3 -D__TARGET_ARCH_x86 -I. -Wall -Wextra
USER_CFLAGS := -std=c17 -Wall -Wextra $(shell pkg-config --cflags libbpf)
USER_LIBS := $(shell pkg-config --libs libbpf)

.PHONY: all combined check clean

all: openstat

vmlinux.h:
	bpftool btf dump file /sys/kernel/btf/vmlinux format c > $@

%.bpf.o: %.bpf.c vmlinux.h
	$(CLANG) $(CFLAGS) -c $< -o $@

openstat.skel.h: openstat.bpf.o
	bpftool gen skeleton $< > $@

openstat: openstat.c openstat.skel.h
	$(CC) $(USER_CFLAGS) $< -o $@ $(USER_LIBS)

# 把两个独立的目标文件静态链接成一个，不需要重新编译。
openstat.combined.bpf.o: openstat.bpf.o syscallstat.bpf.o
	bpftool gen object $@ $^

combined: openstat.combined.bpf.o

check: openstat.bpf.o syscallstat.bpf.o openstat.skel.h
	$(CC) $(USER_CFLAGS) -fsyntax-only openstat.c

clean:
	rm -f openstat openstat.bpf.o syscallstat.bpf.o openstat.combined.bpf.o openstat.skel.h
