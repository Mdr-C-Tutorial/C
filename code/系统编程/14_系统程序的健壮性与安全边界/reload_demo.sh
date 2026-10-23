#!/bin/bash
# 优雅退出与重载演示。
set -u
cd "$(dirname "$0")" || exit 1

printf 'max_conn 8\nmax_line 64\nmax_out 4096\nidle_ms 30000\nbanner lecho ready\n' > reload.conf
rm -f reload.log
./lecho_hardened 21402 reload.conf 2> reload.log &
SRV=$!
sleep 0.3

echo "== 重载：先写坏配置 =="
printf 'max_conn 8\nmax_line abc\n' > reload.conf
kill -HUP "$SRV"
sleep 0.3

echo "== 重载：合法配置 =="
printf 'max_conn 8\nmax_line 16\nmax_out 4096\nidle_ms 30000\nbanner v2\n' > reload.conf
kill -HUP "$SRV"
sleep 0.3
printf 'hello\n' | timeout 4 nc -q 1 127.0.0.1 21402

echo "== 客户端保持连接时收到 SIGTERM =="
python3 - <<'PY' &
import socket, time
s = socket.create_connection(("127.0.0.1", 21402))
s.recv(64)                       # banner
for i in range(20):
    s.sendall(b"line %d\n" % i)
time.sleep(1.0)                  # 等 SIGTERM 到达
data = b""
while True:
    chunk = s.recv(4096)
    if not chunk:
        break
    data += chunk
print("client received", data.count(b"\n"), "lines, then EOF")
PY
CLIENT=$!
sleep 0.5
kill -TERM "$SRV"
wait "$CLIENT"
wait "$SRV"
echo "exit=$?"

echo "== reload.log =="
cat reload.log