#!/bin/sh
# imud 的集成测试：数据源用普通文件（追加即出新样本）+ 本地伪 MQTT 服务器。
#
# 用法（宿主机上，先按 imud/README.md 编好 imud/build/imud 与 scripts/fake_broker）：
#   ./run_demo.sh [端口]
# 只绑 127.0.0.1，结束时按记下的 PID 停掉自己启动的进程。

set -eu

PORT=${1:-25765}
HERE=$(cd "$(dirname "$0")" && pwd)
IMUD="$HERE/../imud/build/imud"
TAG="$HERE/demo$$"
CONF="$TAG.conf"
DATA="$TAG.data"
LEVEL="$TAG.level.bin"
TILT="$TAG.tilt.bin"
BLOG="$TAG.broker.log"
ILOG="$TAG.imud.log"

if [ ! -x "$IMUD" ] || [ ! -x "$HERE/fake_broker" ]; then
    echo "先在 imud/ 下构建，再编 scripts/fake_broker" >&2
    exit 1
fi

rm -f "$CONF" "$DATA" "$LEVEL" "$TILT" "$BLOG" "$ILOG"
: > "$DATA"

# 预先生成两批样本：水平姿态与倾斜 30 度
python3 "$HERE/make_samples.py" -n 100 -r 100 --burst > "$LEVEL"
python3 "$HERE/make_samples.py" -n 100 -r 100 --burst --tilt 30 > "$TILT"

cat > "$CONF" <<CONF
# 集成测试用配置：数据源是一个普通文件，追加数据就是"有新样本"
source = $DATA
host = 127.0.0.1
port = $PORT
device_id = dk1-01
period_ms = 300
sample_rate = 100
alpha = 0.98
CONF

if ss -ltn 2>/dev/null | grep -q ":$PORT "; then
    echo "端口 $PORT 已被占用" >&2
    rm -f "$CONF" "$DATA" "$LEVEL" "$TILT"
    exit 1
fi

"$HERE/fake_broker" "$PORT" > "$BLOG" 2>&1 &
BROKER=$!
sleep 0.3

"$IMUD" -f "$CONF" > "$ILOG" 2>&1 &
IMUD_PID=$!
sleep 0.5

# 第一批：水平姿态，一轮上报后姿态角应为 0
cat "$LEVEL" >> "$DATA"
sleep 1

# 第二批：连续喂 8 批倾斜样本，观察互补滤波逐步收敛到 30 度附近
i=0
while [ "$i" -lt 8 ]; do
    cat "$TILT" >> "$DATA"
    sleep 0.7
    i=$((i + 1))
done
sleep 1

kill -TERM "$IMUD_PID" 2>/dev/null || true
wait "$IMUD_PID" || true
kill -TERM "$BROKER" 2>/dev/null || true
wait "$BROKER" 2>/dev/null || true

echo "=== imud"
cat "$ILOG"
echo "=== broker"
grep PUBLISH "$BLOG" || true

rm -f "$CONF" "$DATA" "$LEVEL" "$TILT" "$BLOG" "$ILOG"