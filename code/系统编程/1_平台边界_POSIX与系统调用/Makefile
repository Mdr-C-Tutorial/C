CC       ?= gcc
CFLAGS   ?= -std=c17 -Wall -Wextra -g
BUILD    := build

# need_macro.c 与 late_macro.c 故意编译不过（前者缺宏，后者宏写在 #include 之后），
# 不列入构建目标；它们的正面对照是 need_macro_posix.c。
PROGS    := hello_printf hello_write hello_syscall raw_syscall \
            need_macro_posix lines many many_write fflush_demo mix vdso_demo \
            clock_syscall errno_demo conventions errno_first retval_first sysinfo

.PHONY: all clean

all: $(addprefix $(BUILD)/,$(PROGS))

$(BUILD):
	mkdir -p $@

$(BUILD)/%: %.c | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@ -pthread

clean:
	rm -rf $(BUILD)
