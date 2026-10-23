# Mdr_Server 的简易 Makefile，给没有安装 CMake 的读者使用。
#
#   make                  构建服务器和两个测试程序（服务器需要 Linux）
#   make test             运行单元测试和集成测试
#   make unit-test        只构建并运行可移植的单元测试（Windows 上也可以）
#   make PORT=20081 test  集成测试改用别的端口
#   make clean
#
# 产物放在 build/make/ 下，和 CMake 的构建目录互不干扰。

BUILD ?= build/make
PORT ?= 20080
CFLAGS ?= -O2 -g
WERROR ?= -Werror
WARNINGS = -std=c11 -Wall -Wextra -Wpedantic $(WERROR)
CPPFLAGS += -Iinclude

ifeq ($(OS),Windows_NT)
EXE := .exe
# MinGW 没有 make 默认使用的 cc，用户没指定 CC 时改用 gcc。
ifeq ($(origin CC),default)
CC := gcc
endif
endif

# Windows 原生的 mingw32-make 只在 PATH 里找得到 sh 时才用它执行命令，在
# cmd 或 PowerShell 里会改用 cmd.exe，mkdir -p、rm -rf 都执行不了。这里
# 干脆固定用 cmd.exe，在哪个终端里运行都一样。cmd.exe 把 '/' 当作选项的
# 开头，路径要换成 '\'。
ifneq ($(findstring mingw32,$(MAKE_HOST)),)
SHELL := cmd.exe
NATIVE = $(subst /,\,$(1))
MKDIR_P = if not exist "$(call NATIVE,$(1))" mkdir "$(call NATIVE,$(1))"
RM_RF = if exist "$(call NATIVE,$(1))" rmdir /s /q "$(call NATIVE,$(1))"
else
NATIVE = $(1)
MKDIR_P = mkdir -p $(1)
RM_RF = rm -rf $(1)
endif

CORE_OBJS := $(BUILD)/buffer.o $(BUILD)/http.o $(BUILD)/timer.o
SERVER_OBJS := $(BUILD)/main.o $(BUILD)/server.o $(BUILD)/connection.o

SERVER := $(BUILD)/mdr_server$(EXE)
UNIT_TEST := $(BUILD)/unit_test$(EXE)
INTEGRATION_TEST := $(BUILD)/integration_test$(EXE)

all: $(SERVER) $(UNIT_TEST) $(INTEGRATION_TEST)

$(SERVER): $(SERVER_OBJS) $(CORE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(UNIT_TEST): $(BUILD)/unit_test.o $(CORE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(INTEGRATION_TEST): $(BUILD)/integration_test.o
	$(CC) $(LDFLAGS) -o $@ $^

# -MMD -MP 让编译器顺便生成 .d 依赖文件，头文件改了，用到它的 .o 会重新编译。
$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(WARNINGS) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD)/%.o: tests/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(WARNINGS) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD):
	$(call MKDIR_P,$@)

test: unit-test integration-test

unit-test: $(UNIT_TEST)
	$(call NATIVE,$(UNIT_TEST))

integration-test: $(SERVER) $(INTEGRATION_TEST)
	$(INTEGRATION_TEST) $(SERVER) $(PORT)

clean:
	$(call RM_RF,$(BUILD))

-include $(wildcard $(BUILD)/*.d)

.PHONY: all test unit-test integration-test clean
