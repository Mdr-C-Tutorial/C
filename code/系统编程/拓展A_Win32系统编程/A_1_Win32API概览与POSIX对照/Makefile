# A.1 的示例。MinGW-w64 GCC 13.2（UCRT），x64。
#
#   mingw32-make            编译全部程序
#   mingw32-make clean      删除编译产物
#
# 两个程序不放进默认目标：
#   winver3.c  需要目标版本宏才能编译（正文里给了命令行）
#   zu.c       故意用 %zu，编译必然报警告，正文用它演示这条警告

CC = gcc
CFLAGS ?= -std=c17 -Wall -Wextra
WMAIN := cpw copyfile
PLAIN := win32types failcheck modules

all: $(addsuffix .exe,$(WMAIN) $(PLAIN))

cpw.exe: cpw.c
	$(CC) $(CFLAGS) -municode $< -o $@

copyfile.exe: copyfile.c
	$(CC) $(CFLAGS) -municode $< -o $@

%.exe: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(addsuffix .exe,$(WMAIN) $(PLAIN))

.PHONY: all clean