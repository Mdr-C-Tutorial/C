# 本页示例。需要 MinGW-w64 GCC（UCRT）与 64 位 Windows。
#
#   mingw32-make          编译全部程序
#   mingw32-make clean    删除编译产物

CC = gcc
CFLAGS = -std=c17 -Wall -Wextra

PROGS := ov_basic.exe ov_chunks.exe readex_copy.exe no_alert.exe \
         sync_complete.exe reuse_ov.exe wait_handle.exe stale_buffer.exe \
         offset_demo.exe ov_pipe_server.exe pipe_client.exe cancel_demo.exe

all: $(PROGS)

# 用 wmain 的程序要加 -municode
UNICODE_PROGS := ov_basic.exe ov_chunks.exe readex_copy.exe no_alert.exe \
                 offset_demo.exe pipe_client.exe reuse_ov.exe \
                 ov_pipe_server.exe

$(UNICODE_PROGS): %.exe: %.c
	$(CC) $(CFLAGS) -municode $< -o $@

%.exe: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	-del /Q $(PROGS)

.PHONY: all clean