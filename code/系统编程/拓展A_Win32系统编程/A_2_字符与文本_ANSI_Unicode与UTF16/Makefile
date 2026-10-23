# A.2 的示例。MinGW-w64 GCC 13.2（UCRT），x64。
#
#   mingw32-make            编译全部程序
#   mingw32-make clean      删除编译产物

CC = gcc
CFLAGS ?= -std=c17 -Wall -Wextra
WINDRES ?= windres

PROGS := ansi_vs_wide wide_bytes mojibake macrochk console_cp wprint \
         args_main launch_args cargv conv badconv

all: $(addsuffix .exe,$(PROGS)) wargs.exe utf8_manifest/acp.exe utf8_manifest/acp_utf8.exe

%.exe: %.c
	$(CC) $(CFLAGS) $< -o $@

wargs.exe: wargs.c
	$(CC) $(CFLAGS) -municode $< -o $@

conv.exe badconv.exe: %.exe: %.c textconv.c
	$(CC) $(CFLAGS) $< textconv.c -o $@

utf8_manifest/utf8.res.o: utf8_manifest/utf8.rc utf8_manifest/utf8.manifest
	$(WINDRES) utf8_manifest/utf8.rc -O coff -o $@

utf8_manifest/acp.exe: utf8_manifest/acp.c
	$(CC) $(CFLAGS) $< -o $@

utf8_manifest/acp_utf8.exe: utf8_manifest/acp.c utf8_manifest/utf8.res.o
	$(CC) $(CFLAGS) $< utf8_manifest/utf8.res.o -o $@

clean:
	rm -f $(addsuffix .exe,$(PROGS)) utf8_manifest/*.exe utf8_manifest/*.o

.PHONY: all clean