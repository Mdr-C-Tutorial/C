# 本章示例。需要 POSIX 环境；timerfd 部分需要 Linux。
#
#   make           编译全部程序
#   make clean     删除编译产物
#
# 每节的命令行用法在正文里给出。

CC ?= gcc
CFLAGS ?= -std=c17 -Wall -Wextra -g

PROGS := graceful siginfo eintr coalesce sigsuspend_wait lost_wakeup rtsig \
         reap execsig elapsed drift ptimer tfd poll_eintr unsafe sleep_left

all: $(PROGS)

$(PROGS): %: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(PROGS)

.PHONY: all clean