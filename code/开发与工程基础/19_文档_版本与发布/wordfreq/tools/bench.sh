#!/bin/sh
# 固定条件的基准测试：Linux 上运行。构建一次 v0.2，对三种规模的输入各跑 5 次，
# 记录 real 时间的中位数。
#
#   sh tools/bench.sh
#
# 依赖：gcc、make（或直接用下面的 gcc 命令），输入由 tools/gen_words 生成。
set -eu

cd "$(dirname "$0")/.."

BUILD=build-bench
CC=${CC:-gcc}
CFLAGS="-std=c17 -O2 -g -Iinclude"

mkdir -p "$BUILD"
$CC $CFLAGS tools/gen_words.c -o "$BUILD/gen_words"
$CC $CFLAGS src/token.c src/counter.c src/report.c app/main.c -o "$BUILD/wordfreq"

for n in 10000 100000 1000000; do
    $BUILD/gen_words 1000 "$n" 42 > "$BUILD/words-$n.txt"
done

echo "words     median real (s)"
for n in 10000 100000 1000000; do
    times=""
    for i in 1 2 3 4 5; do
        start=$(date +%s%N)
        "$BUILD/wordfreq" -n 10 "$BUILD/words-$n.txt" > /dev/null
        end=$(date +%s%N)
        times="$times $(( (end - start) / 1000000 ))"
    done
    median=$(printf '%s\n' $times | sort -n | sed -n 3p)
    printf '%-9s %s ms\n' "$n" "$median"
done
