# 3.2 的对照程序。struct_write 和 bytes_demo 任何平台都能编译；
# 其余几个需要 POSIX 套接字，请在 Linux、macOS 上运行。
CFLAGS ?= -O2 -g
REQUIRED_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic

all: struct_write bytes_demo
posix: fgets_poll recv_zero sigpipe slow_sink flood_sender

struct_write: struct_write.c
	$(CC) $(REQUIRED_CFLAGS) $(CFLAGS) -o $@ $<

fgets_poll: fgets_poll.c
	$(CC) $(REQUIRED_CFLAGS) $(CFLAGS) -o $@ $<

recv_zero: recv_zero.c
	$(CC) $(REQUIRED_CFLAGS) $(CFLAGS) -o $@ $<

sigpipe: sigpipe.c
	$(CC) $(REQUIRED_CFLAGS) $(CFLAGS) -o $@ $<

bytes_demo: bytes_demo.c ../src/protocol.c
	$(CC) $(REQUIRED_CFLAGS) $(CFLAGS) -I../include -o $@ bytes_demo.c \
	      ../src/protocol.c

slow_sink: slow_sink.c
	$(CC) $(REQUIRED_CFLAGS) $(CFLAGS) -o $@ $<

flood_sender: flood_sender.c
	$(CC) $(REQUIRED_CFLAGS) $(CFLAGS) -o $@ $<

clean:
	rm -f struct_write struct_write.exe fgets_poll fgets_poll.exe \
	      recv_zero recv_zero.exe sigpipe slow_sink flood_sender bytes_demo

.PHONY: all posix clean

