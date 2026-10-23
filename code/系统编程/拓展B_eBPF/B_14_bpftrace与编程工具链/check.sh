#!/bin/sh
# 不需要 root 的检查：编译内核态程序、生成骨架、链接目标文件、用户态语法检查。
set -e

make clean
make openstat.bpf.o syscallstat.bpf.o openstat.skel.h openstat.combined.bpf.o
make check
echo "check passed"
