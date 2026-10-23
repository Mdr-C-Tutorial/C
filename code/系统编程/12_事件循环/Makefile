# 本章示例。epoll、signalfd、eventfd 是 Linux 专有接口，只能在 Linux 上运行。
#
#   make           编译全部程序
#   make test      运行三个不依赖服务器的程序
#   make clean     删除编译产物

CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -g
CPPFLAGS =
LDLIBS =

PROGS = timer_demo lecho_evl slow_evl uaf_demo uaf_fixed uaf_timer \
        signalfd_demo selfpipe_demo eventfd_demo eintr_epoll

all: $(PROGS)

timer_demo: timer_demo.c evl.c evl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) timer_demo.c evl.c -o $@

lecho_evl: lecho_evl.c evl.c evl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) lecho_evl.c evl.c -o $@

slow_evl: slow_evl.c evl.c evl.h tpool.c tpool.h
	$(CC) $(CPPFLAGS) $(CFLAGS) slow_evl.c evl.c tpool.c -pthread $(LDLIBS) -o $@

uaf_demo: uaf_demo.c evl.c evl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fsanitize=address uaf_demo.c evl.c -o $@

uaf_fixed: uaf_fixed.c evl.c evl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fsanitize=address uaf_fixed.c evl.c -o $@

uaf_timer: uaf_timer.c evl.c evl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fsanitize=address uaf_timer.c evl.c -o $@

signalfd_demo: signalfd_demo.c evl.c evl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) signalfd_demo.c evl.c -o $@

selfpipe_demo: selfpipe_demo.c evl.c evl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) selfpipe_demo.c evl.c -o $@

eventfd_demo: eventfd_demo.c evl.c evl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) eventfd_demo.c evl.c -pthread $(LDLIBS) -o $@

eintr_epoll: eintr_epoll.c
	$(CC) $(CPPFLAGS) $(CFLAGS) eintr_epoll.c -o $@

test: all
	./timer_demo
	./eintr_epoll
	./uaf_fixed

clean:
	$(RM) $(PROGS)

.PHONY: all test clean