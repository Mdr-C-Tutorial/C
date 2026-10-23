# 本页示例。需要 MinGW-w64 GCC（UCRT）与 64 位 Windows。
#
#   mingw32-make          编译全部程序
#   mingw32-make clean    删除编译产物

CC = gcc
CFLAGS = -std=c17 -Wall -Wextra

PROGS := svc_probe.exe lifecycle.exe echo_svc.exe echo_client.exe svc_query.exe \
         svc_install.exe reg_config.exe reg_errors.exe times.exe sleep_grain.exe \
         sysinfo.exe folders.exe eventlog.exe

all: $(PROGS)

echo_svc.exe: echo_svc.c
	$(CC) $(CFLAGS) $< -o $@ -lws2_32

echo_client.exe: echo_client.c
	$(CC) $(CFLAGS) $< -o $@ -lws2_32

folders.exe: folders.c
	$(CC) $(CFLAGS) $< -o $@ -lole32 -luuid

%.exe: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	-del /Q $(PROGS)

.PHONY: all clean