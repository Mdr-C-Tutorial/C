# 第 9 章习题数据：重新生成 data/ 下的全部文件。
#
#   make data     编译 make_data.c 并重写 data/
#   make clean    删除生成的数据文件与程序

CC ?= gcc
CFLAGS ?= -std=c17 -Wall -Wextra -O2 -g

data: make_data
	mkdir -p data
	./make_data data

make_data: make_data.c
	$(CC) $(CFLAGS) make_data.c -o make_data

clean:
	rm -f make_data
	rm -rf data

.PHONY: data clean