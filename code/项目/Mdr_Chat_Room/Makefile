# 没有 CMake 时的简易构建，需要 GNU make。
#
#   make                  编译服务器、客户端和单元测试
#   make test             运行单元测试和集成测试
#   make unit             只运行单元测试（只用到 ISO C，Windows 上也能运行）
#   make SANITIZE=1 test  加上 AddressSanitizer 和 UBSan 编译并测试（先 make clean）
#   make CC=clang         换一个编译器
#
# 产物都放在 build/ 下，make clean 删除。

CFLAGS ?= -O2 -g
# 必需的选项单独写在 REQUIRED_CFLAGS 里，在命令行上改 CFLAGS 时不会把它们一起覆盖掉。
# -MMD -MP 让编译器顺便写出头文件依赖（.d 文件），改了头文件也会重新编译。
REQUIRED_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Iinclude -MMD -MP

ifdef SANITIZE
# UBSan 默认报告之后继续运行，测试照样通过；-fno-sanitize-recover=all 让它报错即终止
CFLAGS += -fsanitize=address,undefined -fno-sanitize-recover=all
CFLAGS += -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address,undefined
endif

ifeq ($(OS),Windows_NT)
EXE := .exe
# make 内置的 CC 默认值是 cc，MinGW 只提供 gcc
ifeq ($(origin CC),default)
CC := gcc
endif
endif

BUILD := build
CORE := $(BUILD)/protocol.o $(BUILD)/room.o
PROGRAMS := $(BUILD)/chat_server $(BUILD)/chat_client
TESTS := $(BUILD)/test_protocol$(EXE) $(BUILD)/test_room$(EXE)

all: $(PROGRAMS) $(TESTS)

$(BUILD)/chat_server: $(BUILD)/server.o $(BUILD)/net.o $(CORE)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD)/chat_client: $(BUILD)/client.o $(BUILD)/net.o $(BUILD)/protocol.o
	$(CC) $(LDFLAGS) -o $@ $^

# 静态模式规则：目标逐个列在 TESTS 里，test_*.o 就不会被当作中间文件删掉。
$(TESTS): $(BUILD)/test_%$(EXE): $(BUILD)/test_%.o $(CORE)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(REQUIRED_CFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: tests/%.c | $(BUILD)
	$(CC) $(REQUIRED_CFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $@

unit: $(TESTS)
	$(BUILD)/test_protocol$(EXE)
	$(BUILD)/test_room$(EXE)

test: unit $(PROGRAMS)
	bash tests/integration.sh $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: all unit test clean

-include $(wildcard $(BUILD)/*.d)
