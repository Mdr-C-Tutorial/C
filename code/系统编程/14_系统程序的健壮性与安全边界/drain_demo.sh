#!/bin/bash
# 优雅退出：SIGTERM 到达时输出缓冲区里还有积压数据，客户端稍后开始读，
# 应当读到全部响应，然后才收到文件结束。
set -u
cd "$(dirname "$0")" || exit 1

printf 'max_conn 8\nmax_line 4096\nmax_out 33554432\nidle_ms 60000\nbanner ready\n' > drain.conf
rm -f drain.log
./lecho_hardened 21404 drain.conf 2> drain.log &
SRV=$!
sleep 0.3

python3 - <<'PY' &
import socket, time
s = socket.create_connection(("127.0.0.1", 21404))
s.recv(64)
for i in range(12000):
    s.sendall(b"x" * 1000 + b"\n")
time.sleep(1.2)          # 不读：服务器输出缓冲区里积压着待发数据
data = b""
while True:
    chunk = s.recv(65536)
    if not chunk:
        break
    data += chunk
print("client received", data.count(b"\n"), "lines, then EOF")
PY
CLIENT=$!
sleep 0.6
kill -TERM "$SRV"
wait "$CLIENT"
wait "$SRV"
echo "exit=$?"
echo "== drain.log =="
cat drain.log