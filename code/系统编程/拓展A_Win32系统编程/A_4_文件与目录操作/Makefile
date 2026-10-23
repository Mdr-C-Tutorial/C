# 本页示例。需要 MinGW-w64 GCC（UCRT）与 64 位 Windows。
#
#   mingw32-make          编译全部程序
#   mingw32-make clean    删除编译产物

CC = gcc
CFLAGS = -std=c17 -Wall -Wextra

PROGS := disposition.exe records.exe share_demo.exe fileinfo.exe duw.exe \
         atomic_save.exe lock_demo.exe delete_on_close.exe eol.exe pathdemo.exe

all: $(PROGS)

# 用 wmain 的程序要加 -municode
UNICODE_PROGS := fileinfo.exe duw.exe

$(UNICODE_PROGS): %.exe: %.c
	$(CC) $(CFLAGS) -municode $< -o $@

%.exe: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	-del /Q $(PROGS)

.PHONY: all clean