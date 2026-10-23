#!/bin/sh
set -e
gcc -std=c17 -Wall -Wextra -o build/bad src/bad.c
echo "编译成功"
