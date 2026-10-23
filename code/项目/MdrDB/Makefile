# 给没有 CMake 的读者准备的 Makefile，需要 GNU make 和 POSIX shell。
# Windows 上请在 Git Bash 或 MSYS2 里运行 mingw32-make。
#
#   make          构建 build-make/mdrdb 和全部测试
#   make test     构建并运行全部测试
#   make clean    删除构建目录

ifeq ($(origin CC),default)
CC = gcc
endif
CFLAGS ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude -Isrc

BUILD := build-make
ifeq ($(OS),Windows_NT)
EXE := .exe
endif

LIB_SRC := src/btree.c src/db.c src/file.c src/journal.c src/node.c \
           src/pager.c src/record.c src/repl.c
LIB_OBJ := $(LIB_SRC:src/%.c=$(BUILD)/%.o)
HEADERS := $(wildcard include/*.h src/*.h)
TESTS := record node journal pager btree crash repl
TEST_BIN := $(TESTS:%=$(BUILD)/test_%$(EXE))

.PHONY: all test clean

all: $(BUILD)/mdrdb$(EXE) $(TEST_BIN)

$(BUILD):
	mkdir -p $(BUILD)

# 头文件很少，简单起见让每个目标文件依赖全部头文件。
$(BUILD)/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/libmdrdb.a: $(LIB_OBJ)
	$(AR) rcs $@ $^

$(BUILD)/mdrdb$(EXE): src/main.c $(BUILD)/libmdrdb.a $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/main.c $(BUILD)/libmdrdb.a -o $@

$(BUILD)/test_%$(EXE): tests/test_%.c tests/test.h $(BUILD)/libmdrdb.a \
                       $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libmdrdb.a -o $@

# 测试在构建目录里运行，生成的临时数据库文件也留在那里。
test: $(TEST_BIN)
	@cd $(BUILD) && for t in $(TESTS); do \
	    echo "== test_$$t"; ./test_$$t$(EXE) || exit 1; \
	done

clean:
	rm -rf $(BUILD)
