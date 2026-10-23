#!/bin/bash
# 优雅退出：客户端停止读取，服务器有排队输出，宽限期到期后放弃。
set -u
cd "$(dirname "$0")" || exit 1

printf 'max_conn 8\nmax_line 4096\nmax_out 33554432\nidle_ms 60000\nbanner ready\n' > force.conf
rm -f force.log
./lecho_hardened 21403 force.conf 2> force.log &
SRV=$!
sleep 0.3

python3 - <<'PY' &
import socket, time
s = socket.create_connection(("127.0.0.1", 21403))
s.recv(64)
line = b"x" * 1000 + b"\n"
for _ in range(12000):
    s.sendall(line)
print("client: sent 12000 lines, now stops reading")
time.sleep(10)
PY
CLIENT=$!
sleep 1.2
echo "-- SIGTERM，客户端不读，等宽限期 --"
kill -TERM "$SRV"
wait "$SRV"
echo "exit=$?"
kill "$CLIENT" 2>/dev/null
wait "$CLIENT" 2>/dev/null
echo "== force.log =="
cat force.log