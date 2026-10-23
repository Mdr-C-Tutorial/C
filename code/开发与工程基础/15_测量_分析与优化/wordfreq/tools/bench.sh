#!/bin/sh
# wordfreq 的基准脚本：固定输入、固定选项，对三种规模各跑 5 次，取 real 时间的中位数。
#
#   sh tools/bench.sh
#
# 外部计时包含进程启动与 shell 开销，先用一个空文件量出这份固定成本，
# 读数时从各规模的耗时里减去，才能看出真正用于计算的时间。
set -eu

cd "$(dirname "$0")/.."

BUILD=build-bench
CC=${CC:-gcc}
CFLAGS="-std=c17 -O2 -g -Iinclude"

mkdir -p "$BUILD"
$CC $CFLAGS tools/gen_words.c -o "$BUILD/gen_words"
$CC $CFLAGS src/token.c src/counter.c src/report.c app/main.c -o "$BUILD/wordfreq"

for n in 10000 100000 1000000; do
    "$BUILD/gen_words" 1000 "$n" 42 > "$BUILD/words-$n.txt"
done
: > "$BUILD/empty.txt"

# 运行 5 次，打印 real 时间的中位数（毫秒）
median_ms() {
    times=""
    for i in 1 2 3 4 5; do
        start=$(date +%s%N)
        "$@" > /dev/null
        end=$(date +%s%N)
        times="$times $(( (end - start) / 1000000 ))"
    done
    printf '%s\n' $times | sort -n | sed -n 3p
}

baseline=$(median_ms "$BUILD/wordfreq" "$BUILD/empty.txt")

echo "build:    $CFLAGS"
echo "baseline: ${baseline} ms  (empty input: process startup, shell and timer overhead)"
printf '%-9s %9s %9s\n' words total net
for n in 10000 100000 1000000; do
    total=$(median_ms "$BUILD/wordfreq" -n 10 "$BUILD/words-$n.txt")
    printf '%-9s %7s ms %5s ms\n' "$n" "$total" "$((total - baseline))"
done
