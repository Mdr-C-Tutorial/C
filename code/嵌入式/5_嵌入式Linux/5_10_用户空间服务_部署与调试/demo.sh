#!/bin/sh
# 在宿主机上跑一次 sensord：用命名管道作数据源，演示周期输出、
# SIGHUP 重载与 SIGTERM 退出。只结束自己记下的进程。
set -e

DIR=$(dirname "$0")
cd "$DIR"
FIFO=demo.fifo
rm -f "$FIFO" out.log err.log
mkfifo "$FIFO"

cat > demo.conf <<'CONF'
source = demo.fifo
period_ms = 1000
CONF

./sensord -f demo.conf > out.log 2> err.log &
PID=$!
trap 'kill -TERM "$PID" 2>/dev/null || true' EXIT

sleep 1.1
printf '10:00:00 21.5\n10:00:01 22.0\n10:00:02 23.5\n' > "$FIFO"
sleep 1.1
printf '10:00:03 20.4\n10:00:04 25.1\n' > "$FIFO"
sleep 1.1

echo '--- SIGHUP，周期改为 2000 ms'
printf 'source = demo.fifo\nperiod_ms = 2000\n' > demo.conf
kill -HUP "$PID"
sleep 1.1

echo '--- SIGTERM'
kill -TERM "$PID"
wait "$PID" || true
trap - EXIT
rm -f "$FIFO"

echo '--- stdout'
cat out.log
echo '--- stderr'
cat err.log
