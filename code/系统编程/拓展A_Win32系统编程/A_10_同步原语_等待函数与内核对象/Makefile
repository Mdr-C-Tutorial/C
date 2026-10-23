# A.10 的示例。MinGW-w64 GCC 13.2（UCRT），x64。
#
#   mingw32-make            编译全部程序
#   mingw32-make clean      删除编译产物

CC = gcc
CFLAGS ?= -std=c17 -Wall -Wextra

PROGS := wait_multi timeout_granularity auto_reset event_demo stop_event \
         abandoned semaphore_limit timer_tick timer_resolution \
         ready_wait ready_set apc_demo

all: $(addsuffix .exe,$(PROGS))

%.exe: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(addsuffix .exe,$(PROGS))

.PHONY: all clean