# A.3 的示例。MinGW-w64 GCC 13.2（UCRT），x64。
#
#   mingw32-make            编译全部程序
#   mingw32-make clean      删除编译产物

CC = gcc
CFLAGS ?= -std=c17 -Wall -Wextra

PROGS := handles two_handles rights cleanup fail_values pseudo_dup \
         named_objects private_ns single_instance ref_parent ref_child \
         inherit_parent inherit_child dup_parent dup_child leak double_close

all: $(addsuffix .exe,$(PROGS))

%.exe: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(addsuffix .exe,$(PROGS))

.PHONY: all clean