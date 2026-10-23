# wordfreq 的构建文件。
#
#   make        构建 build/wordfreq
#   make test   用样例数据运行，并与期望输出比较
#   make run    用样例数据运行
#   make clean  删除 build/

CC = gcc
AR = ar
CPPFLAGS = -Iinclude -MMD -MP
CFLAGS = -std=c17 -Wall -Wextra
LDFLAGS =
LDLIBS =

SRCS := $(wildcard src/*.c)
LIB_OBJS := $(SRCS:src/%.c=build/%.o)
OBJS := $(LIB_OBJS) build/main.o
DEPS := $(OBJS:.o=.d)

.PHONY: all clean test run

all: build/wordfreq

build/wordfreq: build/main.o build/libwf.a
	$(CC) $(LDFLAGS) -o $@ build/main.o -Lbuild -lwf $(LDLIBS)

# 先删掉旧的归档：ar 只增加或替换成员，不会去掉已经过时的成员
build/libwf.a: $(LIB_OBJS)
	$(RM) $@
	$(AR) rcs $@ $^

build/%.o: src/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/main.o: app/main.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# 只作先决条件使用的目录：写成 order-only，目录的时间戳变化不触发重编
build:
	mkdir -p $@

# Windows 上程序的 stdout 是文本模式，行尾为 CRLF；去掉回车再比较
test: build/wordfreq
	@build/wordfreq -n 3 tests/data/sample.txt | tr -d '\r' | diff -u tests/data/sample.expected -
	@echo "sample: ok"

run: build/wordfreq
	@build/wordfreq tests/data/sample.txt

clean:
	$(RM) -r build

-include $(DEPS)
