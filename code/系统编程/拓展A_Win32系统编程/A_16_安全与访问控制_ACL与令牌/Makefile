# 本页示例。需要 MinGW-w64 GCC（UCRT）与 64 位 Windows。
#
#   mingw32-make          编译全部程序
#   mingw32-make clean    删除编译产物

CC = gcc
CFLAGS = -std=c17 -Wall -Wextra

PROGS := tokeninfo.exe deny_only_group.exe low_integrity.exe integrity_label.exe \
         access_mask.exe order_demo.exe access_check.exe sddl_show.exe dacl_kinds.exe \
         private_file.exe private_pipe.exe add_ace.exe default_dacl.exe privilege.exe \
         adjust_ret.exe token_member.exe restrict_token.exe check_once.exe

all: $(PROGS)

%.exe: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	-del /Q $(PROGS)

.PHONY: all clean