# 第 13 章的示例程序。在 Linux（或 WSL2、虚拟机）上运行：
#
#   make            构建全部程序
#   make clean      删除构建结果
#
# ptyrun 是本章的驱动器：它在新建的伪终端里运行另一个程序，并把标准输入
# 转给伪终端主设备，所以没有真实终端也能演示按键与作业控制。

CC ?= gcc
CFLAGS ?= -std=c17 -Wall -Wextra -g

PROGS := ttyinfo canon vtmode getpass_lite keys winsize opost no_restore echoflag rawstop ptyrun jsh eio_test hup_demo

.PHONY: all clean

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(PROGS)