# A.8 的示例。MinGW-w64 GCC 13.2（UCRT），x64。
#
#   mingw32-make            编译全部程序
#   mingw32-make clean      删除编译产物

CC = gcc
CFLAGS = -std=c17 -Wall -Wextra

# 用 wmain 的程序需要 -municode。
UNICODE_PROGS := child run showenv envblock mini_ps job_kill parallel procinfo
ASCII_PROGS := spawn cp_literal quote_demo rules_demo busy cleanup_child \
               terminate zombie handle_leak orphan nojob sleeper

PROGS := $(addsuffix .exe,$(UNICODE_PROGS) $(ASCII_PROGS))

all: $(PROGS)

$(addsuffix .exe,$(UNICODE_PROGS)): %.exe: %.c
	$(CC) $(CFLAGS) -municode $< -o $@

$(addsuffix .exe,$(ASCII_PROGS)): %.exe: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	-del /Q $(PROGS)

.PHONY: all clean