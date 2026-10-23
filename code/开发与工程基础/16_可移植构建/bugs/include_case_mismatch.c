/*
 * 第 16 章的事件 (b)：include 的大小写写错。
 * 文件系统区分大小写的系统（Linux、macOS 默认）上找不到 Counter.h，
 * Windows 与默认配置的 macOS 上能编译。
 *
 * 把它单独编译即可：
 *   gcc -std=c17 -Wall -Wextra -Iinclude -c bugs/include_case_mismatch.c
 */
#include "wf/Counter.h"

int use_counter_case_demo(void) { return wf_counter_size(wf_counter_create()); }
