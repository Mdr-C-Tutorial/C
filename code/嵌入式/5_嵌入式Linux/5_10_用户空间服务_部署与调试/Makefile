# 5.10 的示例。sensord 只用 POSIX 与 epoll、signalfd，需要在 Linux 上运行。
#
#   make           编译全部
#   make test      跑一次使用普通文件作数据源的会话
#   make clean     删除编译产物

CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -O2
PROGS = sensord watchdog_keepalive notify_demo

all: $(PROGS)

sensord: sensord.c evl.c evl.h sdnotify.c sdnotify.h
	$(CC) $(CFLAGS) sensord.c evl.c sdnotify.c -o $@

watchdog_keepalive: watchdog_keepalive.c
	$(CC) $(CFLAGS) watchdog_keepalive.c -o $@

notify_demo: notify_demo.c sdnotify.c sdnotify.h
	$(CC) $(CFLAGS) notify_demo.c sdnotify.c -o $@

test: sensord
	./demo.sh

clean:
	$(RM) $(PROGS)

.PHONY: all test clean
