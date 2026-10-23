#!/bin/sh
# build.sh：编译本目录下的两个程序并试运行
set -e
mkdir -p build
gcc -std=c17 -Wall -Wextra -o build/hello src/hello.c
gcc -std=c17 -Wall -Wextra -o build/args src/args.c
./build/hello
./build/args first "second argument"
