# A.5 的示例。分成两组：
#   MinGW 组（默认目标）用 MinGW-w64 GCC 13.2（UCRT）编译；
#   MSVC 组用 cl /nologo /W4 /std:c17 编译，命令行写法见 README.md。
#
#   mingw32-make            编译 MinGW 组
#   mingw32-make clean      删除编译产物

CC = gcc
CFLAGS = -std=c17 -Wall -Wextra

PROGS := lasterror.exe lasterror_thread.exe lasterror_overwrite.exe \
         errcodes.exe crash.exe veh_dump.exe

all: $(PROGS)

%.exe: %.c
	$(CC) $(CFLAGS) $< -o $@

veh_dump.exe: veh_dump.c
	$(CC) $(CFLAGS) $< -o $@ -ldbghelp

clean:
	-rm -f $(PROGS)

.PHONY: all clean