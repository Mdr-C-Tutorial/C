#!/bin/sh
# setmax_test.sh —— 对 SETMAX 的 redis-cli 期望输出测试。
#
# 用法：sh setmax_test.sh <redis-server> <redis-cli> [端口]
# 脚本自己启动一个只绑 127.0.0.1 的实例，跑完后按 PID 结束。

SERVER=${1:?usage: setmax_test.sh <redis-server> <redis-cli> [port]}
CLI=${2:?usage: setmax_test.sh <redis-server> <redis-cli> [port]}
PORT=${3:-27200}
WORK=$(mktemp -d)
FAILED=0

fail() {
    echo "FAIL: $*"
    FAILED=$((FAILED + 1))
}

check_error() {
    got=$("$CLI" -p "$PORT" "$@" 2>&1)
    case $got in
    *"value is not an integer"*) echo "ok   $* -> $got" ;;
    *) fail "$* -> '$got'，期望整数解析错误" ;;
    esac
}

expect() {
    # expect <期望> <命令...>
    want=$1
    shift
    got=$("$CLI" -p "$PORT" "$@" 2>&1)
    if [ "$got" = "$want" ]; then
        echo "ok   $* -> $got"
    else
        fail "$* -> '$got'，期望 '$want'"
    fi
}

"$SERVER" --port "$PORT" --bind 127.0.0.1 --save "" --appendonly no \
    --dir "$WORK" --logfile "$WORK/server.log" &
SPID=$!
trap 'kill $SPID 2>/dev/null; wait $SPID 2>/dev/null; rm -rf "$WORK"' EXIT INT TERM

i=0
while ! "$CLI" -p "$PORT" ping >/dev/null 2>&1; do
    i=$((i + 1))
    if [ $i -gt 50 ]; then
        echo "server did not start, see $WORK/server.log"
        exit 1
    fi
    sleep 0.1
done

expect 1 setmax counter 10
expect '10' get counter
expect 0 setmax counter 5
expect '10' get counter
expect 1 setmax counter 15
expect '15' get counter
expect 0 setmax counter 15
expect 1 setmax fresh 7
expect '7' get fresh
expect 'int' object encoding fresh
expect 1 setmax neg -5
expect '-5' get neg
check_error setmax counter notanumber
check_error setmax counter 3.5

if [ $FAILED -ne 0 ]; then
    echo "$FAILED check(s) failed"
    exit 1
fi
echo "all checks passed"
