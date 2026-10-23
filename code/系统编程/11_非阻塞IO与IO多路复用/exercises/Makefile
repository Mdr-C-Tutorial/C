# 习题用到的起始代码与压测脚本。
#
#   make           编译 kvserver_iter 和 kv_client
#   make clean     删除编译产物
#
# 压测：先运行 ./kvserver_iter 21190，再 sh kvstress.sh 21190。
# （kvstress.sh 在 SignalX 的工作目录里运行，路径按需修改。）

CC ?= gcc
CFLAGS ?= -std=c17 -Wall -Wextra -g

all: kvserver_iter kv_client

kvserver_iter: kvserver_iter.c
	$(CC) $(CFLAGS) $< -o $@

kv_client: kv_client.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f kvserver_iter kv_client

.PHONY: all clean
