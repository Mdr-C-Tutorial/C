#!/bin/sh
# 同时运行两份 aslr，并关掉地址随机化，观察同一个虚拟地址在两边的值
setarch "$(uname -m)" -R ./aslr 11 > a.out &
p1=$!
setarch "$(uname -m)" -R ./aslr 22 > b.out
wait "$p1"
echo "进程 1（g_counter = 11）："
cat a.out
echo "进程 2（g_counter = 22）："
cat b.out
