#!/bin/bash
# lecho 加固版演示。只操作工作目录与 21400 端口。
set -u
cd "$(dirname "$0")" || exit 1

printf 'max_conn 3\nmax_line 64\nmax_out 128\nidle_ms 1500\nbanner lecho ready\n' > lecho.conf
rm -f demo.log
./lecho_hardened 21400 lecho.conf 2> demo.log &
SRV=$!
sleep 0.3

echo "== 连接数上限 =="
python3 - <<'PY'
import socket, time
held = []
for _ in range(3):
    s = socket.create_connection(("127.0.0.1", 21400))
    s.recv(64)
    held.append(s)
time.sleep(0.3)
extra = socket.create_connection(("127.0.0.1", 21400))
print("4th client got:", extra.recv(64).decode().strip())
extra.close()
for s in held:
    s.close()
time.sleep(0.2)
PY

echo "== 单行上限 =="
python3 - <<'PY'
import socket
s = socket.create_connection(("127.0.0.1", 21400))
s.recv(64)
s.sendall(b"A" * 200 + b"\n")
print("server says:", s.recv(64).decode().strip())
s.close()
PY

echo "== 输出缓冲上限与背压 =="
python3 - <<'PY'
import socket
s = socket.create_connection(("127.0.0.1", 21400))
s.recv(64)
total = 0
try:
    for _ in range(40):
        s.sendall(b"x" * 60 + b"\n")
        total += 61
except OSError as e:
    print("client error after", total, "bytes:", e)
s.close()
PY
sleep 0.3

echo "== 空闲超时（idle_ms=1500）=="
python3 - <<'PY'
import socket, time
s = socket.create_connection(("127.0.0.1", 21400))
print("banner:", s.recv(64).decode().strip())
time.sleep(2.5)
s.settimeout(1)
print("after idle, recv:", s.recv(64))
PY
sleep 0.3

echo "== SIGHUP 重载 =="
printf 'max_conn 3\nmax_line 8\nmax_out 128\nidle_ms 1500\nbanner hello again\n' > lecho.conf
kill -HUP "$SRV"
sleep 0.3
printf 'ok\n' | timeout 4 nc -q 1 127.0.0.1 21400

echo "== SIGTERM 优雅退出 =="
kill -TERM "$SRV"
for _ in $(seq 1 30); do
    kill -0 "$SRV" 2>/dev/null || break
    sleep 0.1
done
wait "$SRV"
echo "exit=$?"
echo "== demo.log =="
cat demo.log