#!/bin/sh
# 用 nc 测试 lecho_evl：回显、空闲超时、SIGTERM 优雅退出。
# 用法：./test_lecho.sh [端口]
set -u

PORT=${1:-21200}
IDLE=1500

./lecho_evl "$PORT" "$IDLE" > lecho.log 2>&1 &
SERVER=$!
sleep 0.3

echo "== 回显 =="
printf 'hello\nworld\nquit\n' | nc -q 1 127.0.0.1 "$PORT"

echo "== 空闲超时（${IDLE} ms 内没有输入就关闭）=="
(sleep 2.5) | nc 127.0.0.1 "$PORT" > /dev/null &
sleep 2.2

echo "== SIGTERM =="
printf 'still here\n' | nc -q 1 127.0.0.1 "$PORT" &
sleep 0.2
kill -TERM "$SERVER"
wait "$SERVER"
echo "退出码 $?"
cat lecho.log