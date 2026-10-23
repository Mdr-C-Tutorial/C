# 本章示例。需要 Linux（epoll 部分）与 POSIX 环境。
#
#   make           编译全部程序
#   make clean     删除编译产物
#
# 每节的命令行用法在正文里给出。

CC ?= gcc
CFLAGS ?= -std=c17 -Wall -Wextra -g

PROGS := nonblock_read busy_read readiness select_smoke lt_et \
         lecho_iter lecho_poll lecho_epoll pollout_busy tcping \
         holdopen sendline

all: $(PROGS)

$(PROGS): %: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(PROGS)

.PHONY: all clean