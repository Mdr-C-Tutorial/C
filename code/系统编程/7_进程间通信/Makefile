# 第 7 章的示例程序。需要 Linux（memfd 部分）或其它 POSIX 环境：
#
#   make            构建全部程序
#   make clean      删除构建结果
#
# 管道、FIFO、共享内存和锁的演示都只用到 POSIX 接口，进程之间不经过网络。
# 每节的命令行用法在正文里给出。

CC ?= gcc
CFLAGS ?= -std=c17 -Wall -Wextra -g

PROGS := pipe_basic pipe_hang pipe_eof pipe_capacity pipe_atomic \
         pipeline sigpipe popen popen_shell \
         fifo_server fifo_client \
         shm_anon filemap_writer filemap_reader memfd \
         shm_counter ringbuf \
         sem_named shm_posix mq_demo sysv_demo \
         single_instance fcntl_pitfall

.PHONY: all clean

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@

fifo_server fifo_client: fifo_rec.h

clean:
	rm -f $(PROGS)