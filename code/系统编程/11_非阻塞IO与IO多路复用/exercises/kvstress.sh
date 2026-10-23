#!/bin/sh
# 并发压测：20 个客户端各执行 100 条 SET 和 100 条 GET，校验应答。
# 用法：sh kvstress.sh 程序 端口
prog=$1
port=$2
timeout 120 ./"$prog" "$port" > kvserver.log 2>&1 &
SPID=$!
sleep 0.4
pids=""
for i in $(seq 20); do
    timeout 60 ./kv_client "$port" "$i" > "kv$i.out" 2>&1 &
    pids="$pids $!"
done
fail=0
for i in $(seq 20); do
    set -- $pids
    pid=$1
    shift
    pids=$*
    if ! wait "$pid"; then
        echo "client $i 失败：$(cat kv$i.out)"
        fail=$((fail+1))
    fi
done
echo "20 个客户端，失败 $fail 个"
kill $SPID 2>/dev/null
wait $SPID 2>/dev/null
exit $fail
