# kilo 的构建与伪终端回归测试
#
#   make                 编译 kilo、kilo_drive 与 tty_probe
#   make test            用已构建的 kilo 跑 tests/ 下的用例
#   make test-ln         跑 tests-ln/ 下的用例（用于加了行号功能的版本）
#   make KILO=./kilo-fixed test
#
# 每个用例由三个文件组成：NAME.keys 是按键脚本，NAME.start 是打开时的
# 文件内容，NAME.expect 是期望的屏幕末帧。驱动程序把实际屏幕的最后一帧
# 与期望文件逐行比较，不一致时打印 diff 并以非 0 退出。
#
# 用例的输入文件在工作目录里统一叫 NAME.c：kilo 按扩展名选择语法高亮，
# 扩展名变了就绕开了 editorUpdateSyntax，删行用例也就测不到 idx 错误。
#
# 这个文件放在 kilo 源码旁边使用，kilo 的编译规则与上游 Makefile 相同。

CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -g -O1
KILO_CFLAGS ?= -Wall -W -pedantic -std=c99

KILO ?= ./kilo
DRIVER := kilo_drive
PROBE := tty_probe

.PHONY: all test test-ln clean

all: kilo $(DRIVER) $(PROBE)

kilo: kilo.c
	$(CC) -o kilo kilo.c $(KILO_CFLAGS)

$(DRIVER): kilo_drive.c
	$(CC) $(CFLAGS) kilo_drive.c -o $@ -lutil

$(PROBE): tty_probe.c
	$(CC) $(CFLAGS) tty_probe.c -o $@ -lutil

define run_suite
	@mkdir -p build/$(1); \
	fail=0; \
	for keys in $(2)/*.keys; do \
	    name=$${keys%.keys}; \
	    base=$${name##*/}; \
	    out=build/$(1)/$$base.last; \
	    cp $$name.start build/$(1)/$$base.c; \
	    if ./$(DRIVER) -l $$out -t 15000 $$keys -- $(KILO) build/$(1)/$$base.c; then \
	        if diff -u $$name.expect $$out > build/$(1)/$$base.diff; then \
	            echo "PASS $$base"; \
	        else \
	            echo "FAIL $$base"; cat build/$(1)/$$base.diff; fail=1; \
	        fi; \
	    else \
	        echo "FAIL $$base (driver)"; fail=1; \
	    fi; \
	done; \
	exit $$fail
endef

test: $(DRIVER)
	$(call run_suite,plain,tests)

test-ln: $(DRIVER)
	$(call run_suite,ln,tests-ln)

clean:
	rm -rf kilo $(DRIVER) $(PROBE) build