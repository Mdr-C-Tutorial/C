# 本页示例。需要 MinGW-w64 GCC（UCRT）与 64 位 Windows。
#
#   mingw32-make          编译全部程序
#   mingw32-make clean    删除编译产物

CC = gcc
CFLAGS = -std=c17 -Wall -Wextra
LDLIBS = -lws2_32

PROGS := iocp_queue.exe iocp_echo.exe concurrency_probe.exe abort_probe.exe \
         sync_success_probe.exe slow_client.exe idle_client.exe \
         tp_work.exe tp_timer.exe tp_wait.exe tp_io.exe

all: $(PROGS)

%.exe: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

clean:
	-del /Q $(PROGS)

.PHONY: all clean