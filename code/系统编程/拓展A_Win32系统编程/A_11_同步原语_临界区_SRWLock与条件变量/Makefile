# 本页示例。需要 MinGW-w64 GCC（UCRT）与 64 位 Windows。
#
#   mingw32-make          编译全部程序
#   mingw32-make clean    删除编译产物

CC = gcc
CFLAGS = -std=c17 -Wall -Wextra

PROGS := race.exe interlocked.exe interlocked_ret.exe cs_counter.exe cs_recursive.exe \
         lock_cost.exe srw_try.exe srw_deadlock.exe config_cache.exe bounded_queue.exe \
         if_vs_while.exe once.exe wait_on_address.exe

all: $(PROGS)

# 数据竞争要看到错误结果，必须关掉优化：-O2 会把整个循环合并成一次加法。
race.exe: race.c
	$(CC) $(CFLAGS) -O0 $< -o $@

wait_on_address.exe: wait_on_address.c
	$(CC) $(CFLAGS) $< -o $@ -lsynchronization

%.exe: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	-del /Q $(PROGS)

.PHONY: all clean