# 第 3 章的示例程序。在 Linux（或 WSL2、虚拟机）上运行：
#
#   make            构建全部程序
#   make clean      删除构建结果
#
# 每个程序的用法在正文里给出。mystat、myls、dirstat 只用 POSIX 接口；
# mydu 需要 _XOPEN_SOURCE 700（nftw）；watch、watchsave 是 Linux 专有的 inotify。

CC ?= gcc
CFLAGS ?= -std=c17 -Wall -Wextra -g

PROGS := mystat ftype mksock unlink_open myls mydu dirstat umask_demo \
         trunc_demo atomic_save stamp fsinfo devs watch watchsave

.PHONY: all clean

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(PROGS)