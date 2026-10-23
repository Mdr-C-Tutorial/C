# wordfreq 的构建文件。
#
#   make        构建 build/wordfreq
#   make test   运行单元测试与端到端比较
#   make run    用样例数据运行
#   make clean  删除 build/
#
# 这份文件描述第 8 章的构建图，只作对照；本章之后的正式构建以 CMake 为准。
# config.h 与 version.h 本来由 CMake 生成，这里用一条简单规则从模板造出来，
# 让 gcc 直接编译也能通过。

CC = gcc
AR = ar
CPPFLAGS = -Iinclude -Ibuild/include -MMD -MP
CFLAGS = -std=c17 -Wall -Wextra
LDFLAGS =
LDLIBS =

SRCS := $(wildcard src/*.c)
LIB_OBJS := $(SRCS:src/%.c=build/%.o)
OBJS := $(LIB_OBJS) build/main.o
DEPS := $(OBJS:.o=.d) build/check.d build/test_token.d build/test_counter.d

TESTS := build/test_token build/test_counter

.PHONY: all clean test run

all: build/wordfreq

build/wordfreq: build/main.o build/libwf.a
	$(CC) $(LDFLAGS) -o $@ build/main.o -Lbuild -lwf $(LDLIBS)

# 先删掉旧的归档：ar 只增加或替换成员，不会去掉已经过时的成员
build/libwf.a: $(LIB_OBJS)
	$(RM) $@
	$(AR) rcs $@ $^

build/%.o: src/%.c | build build/include/wf/config.h build/include/wf/version.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/main.o: app/main.c | build build/include/wf/config.h build/include/wf/version.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/check.o: tests/check.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/test_token: tests/test_token.c build/check.o build/libwf.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_token.c build/check.o -Lbuild -lwf

build/test_counter: tests/test_counter.c build/check.o build/libwf.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_counter.c build/check.o -Lbuild -lwf

# 只作先决条件使用的目录：写成 order-only，目录的时间戳变化不触发重编
build:
	mkdir -p $@

# CMake 用 configure_file 生成这两个头文件；这里只造出能编译的最小内容：
# 不启用 getline（走 fgets 路径）。版本号从 CMakeLists.txt 的 project(VERSION)
# 读出来，Makefile 里不另写一份。
PROJECT_VERSION := $(shell sed -n 's/^project(wordfreq VERSION \([0-9][0-9.]*\) LANGUAGES C)/\1/p' CMakeLists.txt)
VERSION_PARTS := $(subst ., ,$(PROJECT_VERSION))

build/include/wf/config.h: | build
	mkdir -p build/include/wf
	printf '/* 由 Makefile 生成：不启用 getline */\n' > $@

build/include/wf/version.h: include/wf/version.h.in CMakeLists.txt | build
	mkdir -p build/include/wf
	sed -e 's/@PROJECT_VERSION_MAJOR@/$(word 1,$(VERSION_PARTS))/' \
	    -e 's/@PROJECT_VERSION_MINOR@/$(word 2,$(VERSION_PARTS))/' \
	    -e 's/@PROJECT_VERSION_PATCH@/$(word 3,$(VERSION_PARTS))/' \
	    -e 's/@PROJECT_VERSION@/$(PROJECT_VERSION)/' $< > $@

# 先跑两个单元测试程序，再做端到端比较；
# Windows 上程序的 stdout 是文本模式，行尾为 CRLF，去掉回车再比较
test: build/wordfreq $(TESTS)
	@build/test_token
	@build/test_counter
	@build/wordfreq -n 3 tests/data/sample.txt | tr -d '\r' | diff -u tests/data/sample.expected -
	@echo "sample: ok"

run: build/wordfreq
	@build/wordfreq tests/data/sample.txt

clean:
	$(RM) -r build

-include $(DEPS)
