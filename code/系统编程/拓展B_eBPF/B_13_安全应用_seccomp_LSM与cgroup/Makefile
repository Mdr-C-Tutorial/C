# B.13 的三个示例。vmlinux.h 与骨架都在构建时生成，不入库。
#
#   make            全部构建
#   make seccomp_deny  只构建纯用户态的那个
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

BPF_OBJS = lsm_bpf.bpf.o connect_deny.bpf.o
SKELS = lsm_bpf.skel.h connect_deny.skel.h

all: seccomp_deny lsm_bpf connect_deny

vmlinux.h:
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

%.bpf.o: %.bpf.c vmlinux.h
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

%.skel.h: %.bpf.o
	$(BPFTOOL) gen skeleton $< name $(basename $(basename $<)) > $@

seccomp_deny: seccomp_deny.c
	$(CC) $(CFLAGS) $< -o $@

lsm_bpf: lsm_bpf.c lsm_bpf.skel.h
	$(CC) $(CFLAGS) $(LIBBPF_CFLAGS) $< -o $@ $(LIBBPF_LIBS)

connect_deny: connect_deny.c connect_deny.skel.h
	$(CC) $(CFLAGS) $(LIBBPF_CFLAGS) $< -o $@ $(LIBBPF_LIBS)

clean:
	rm -f seccomp_deny lsm_bpf connect_deny
	rm -f $(BPF_OBJS) $(SKELS) vmlinux.h

.PHONY: all clean
