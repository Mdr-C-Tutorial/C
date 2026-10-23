#!/bin/sh
# git bisect run 用的检查脚本：构建并运行全部测试。
#
#   exit 0    这个提交是好的
#   exit 1    这个提交是坏的（测试失败）
#   exit 125  这个提交无法判定（构建失败），bisect 会跳过它
#
# 在仓库根目录执行，构建目录放在 build-bisect/ 下，不污染 build/。
set -u

BUILD=build-bisect
CC=${CC:-gcc}
# MinGW 的 -Wformat 不认识 %zu，加上这个宏避免无谓的警告噪声
CFLAGS="-std=c17 -Wall -Wextra -Iinclude -D__USE_MINGW_ANSI_STDIO=1"

rm -rf "$BUILD"
mkdir -p "$BUILD"

# 不走 CMake，直接给 config.h 一个保守的值：不定义 getline，用 fgets 路径，
# 任何平台都能编译
mkdir -p "$BUILD/include/wf"
printf '/* 由 check.sh 生成：不启用 getline */\n' > "$BUILD/include/wf/config.h"
CFLAGS="$CFLAGS -I$BUILD/include"

compile() {
    $CC $CFLAGS src/token.c src/counter.c src/report.c app/main.c -o "$BUILD/wordfreq" || exit 125
    $CC $CFLAGS tests/test_token.c tests/check.c src/token.c src/counter.c src/report.c \
        -o "$BUILD/test_token" || exit 125
    $CC $CFLAGS tests/test_counter.c tests/check.c src/token.c src/counter.c src/report.c \
        -o "$BUILD/test_counter" || exit 125
}

compile

"$BUILD/test_token" || exit 1
"$BUILD/test_counter" || exit 1
"$BUILD/wordfreq" -n 3 tests/data/sample.txt | tr -d '\r' | diff -u tests/data/sample.expected - || exit 1

exit 0
