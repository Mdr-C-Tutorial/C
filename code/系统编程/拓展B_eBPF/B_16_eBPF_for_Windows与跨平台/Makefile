# B.16 的示例。Linux 内核态程序用 clang，Windows 侧头文件路径由 EBPFW_INCLUDE 给出。
#
#   make linux                    编译 Linux 侧的 connect_deny 与 helper_id
#   make windows EBPFW_INCLUDE=...  编译 Windows 侧的三份
#   make users EBPFW_INCLUDE=...   编译两个用户态小程序
#   make clean
#
# Linux 侧需要 vmlinux.h 与 libbpf 的 bpf/ 头文件；它们不入库，
# 按 B.2 的办法生成或从 tools/libbpf 取。

CC ?= cc
CLANG ?= clang
CFLAGS = -std=c17 -Wall -Wextra
BPFFLAGS = -target bpf -Wall -Wextra -Werror -O2

.PHONY: all linux windows users clean

all: linux

linux: connect_deny.bpf.o helper_linux.o

connect_deny.bpf.o: connect_deny.bpf.c connect_deny.h
	$(CLANG) $(BPFFLAGS) -g -I. -c $< -o $@

helper_linux.o: helper_id.c
	$(CLANG) $(BPFFLAGS) -c $< -o $@

# Windows 侧的规则都要 EBPFW_INCLUDE，没设就直接报错退出
windows:
	@test -n "$(EBPFW_INCLUDE)" || { echo "需要 EBPFW_INCLUDE=<ebpf-for-windows>\\include"; exit 1; }
	$(CLANG) $(BPFFLAGS) -g -DMDR_PLATFORM_WINDOWS -I$(EBPFW_INCLUDE) -I. -c connect_deny.bpf.c -o connect_deny_win.bpf.o
	$(CLANG) $(BPFFLAGS) -DMDR_PLATFORM_WINDOWS -I$(EBPFW_INCLUDE) -c helper_id.c -o helper_windows.o

users:
	@test -n "$(EBPFW_INCLUDE)" || { echo "需要 EBPFW_INCLUDE=<ebpf-for-windows>\\include"; exit 1; }
	$(CC) $(CFLAGS) -I$(EBPFW_INCLUDE) ctx_layout.c -o ctx_layout.exe
	$(CC) $(CFLAGS) -I$(EBPFW_INCLUDE) verdict.c -o verdict.exe

clean:
	rm -f connect_deny.bpf.o connect_deny_win.bpf.o helper_linux.o helper_windows.o \
	      ctx_layout.exe verdict.exe