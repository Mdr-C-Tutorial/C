# A.9 的示例。MinGW-w64 GCC 13.2（UCRT），x64。
#
#   mingw32-make            编译全部程序
#   mingw32-make clean      删除编译产物
#
# dtls_probe.exe 单独用下面的命令编译，编译器会对它给出 -Wattributes 警告，
# 这正是第 5.1 节要演示的现象，所以不放进 all：
#   gcc -std=c17 -Wall -Wextra -o dtls_probe.exe dtls_probe.c

CC = gcc
CFLAGS ?= -std=c17 -Wall -Wextra -O2

PROGS := create_threads primes_mt thread_name tls_demo pass_index_bug \
         terminate_demo

all: $(addsuffix .exe,$(PROGS))

%.exe: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(addsuffix .exe,$(PROGS)) dtls_probe.exe

.PHONY: all clean