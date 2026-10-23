#!/bin/sh
# 手工构建 wordfreq：把每个 .c 文件分别编译成 build/ 下的目标文件，
# 再把四个目标文件链接成可执行文件。
# 在项目根目录执行：sh build_by_hand.sh
set -e # 任何一条命令失败就停止
set -x # 执行每条命令之前先把它打印出来

mkdir -p build
gcc -std=c17 -Wall -Wextra -Iinclude -c src/token.c -o build/token.o
gcc -std=c17 -Wall -Wextra -Iinclude -c src/counter.c -o build/counter.o
gcc -std=c17 -Wall -Wextra -Iinclude -c src/report.c -o build/report.o
gcc -std=c17 -Wall -Wextra -Iinclude -c app/main.c -o build/main.o
gcc build/main.o build/token.o build/counter.o build/report.o -o build/wordfreq
