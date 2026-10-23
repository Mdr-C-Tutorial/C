#!/usr/bin/env bash
# 集成测试：启动真实的服务器和几个客户端进程，经过 TCP 收发消息，检查各自的输出。
#
# 用法：tests/integration.sh 可执行文件所在目录
# 端口默认 24680，可用环境变量 CHAT_TEST_PORT 指定。
#
# 每个客户端的标准输入接到一个命名管道（FIFO），脚本往管道里写命令，就像有人
# 在键盘上打字；客户端的输出写到文件里，脚本在文件里等待期望的那一行出现。
# 原始协议字节用 bash 的 /dev/tcp 直接发送，用来测试拆包、粘包和坏帧。
# 任何一项检查失败，脚本就打印相关输出并以状态 1 退出。

set -u

bin_dir=${1:?usage: integration.sh BIN_DIR}
port=${CHAT_TEST_PORT:-24680}
work=$(mktemp -d "${TMPDIR:-/tmp}/chat_test.XXXXXX")
pids=()

# 每个进程都套上 timeout（如果有的话），即使脚本中途被杀，它们也不会一直留着。
# 没有 timeout 时用 env 原样运行命令。
guard=(env)
if command -v timeout >/dev/null 2>&1; then
    guard=(timeout 60)
fi

cleanup() {
    exec 3>&- 4>&- 5>&- 6>&-
    for pid in "${pids[@]}"; do
        kill "$pid" 2>/dev/null
    done
    wait 2>/dev/null
    rm -rf "$work"
}
trap cleanup EXIT
trap 'exit 1' INT TERM

# fail 说明 [文件]：打印失败原因和文件内容，然后退出。
fail() {
    echo "FAIL: $1"
    if [ $# -ge 2 ]; then
        sed 's/^/    | /' "$2"
    fi
    exit 1
}

# wait_for 文件 文本：最多等 5 秒，直到文件里出现这段文本（按字面比较）。
# 等待输出出现而不是固定地 sleep：机器快时不浪费时间，机器慢时也不误报。
wait_for() {
    local i
    for ((i = 0; i < 50; i++)); do
        if grep -qF -- "$2" "$1" 2>/dev/null; then
            return 0
        fi
        sleep 0.1
    done
    fail "'$2' did not appear in $(basename "$1")" "$1"
}

# start_client 文件名 文件描述符 昵称
start_client() {
    mkfifo "$work/$1.in"
    # 子进程会继承脚本此时打开的所有描述符，其中包括先启动的客户端的管道写端。
    # 不关掉它们，脚本关闭 bob 的写端时，管道仍有别的写者，bob 永远读不到
    # 输入结束。所以先在子进程里关闭 3~6 号描述符。
    "${guard[@]}" "$bin_dir/chat_client" -p "$port" "$3" 3>&- 4>&- 5>&- 6>&- \
        <"$work/$1.in" >"$work/$1.out" 2>&1 &
    pids+=($!)
    # 打开管道的写端。写端一直开着，客户端就一直等输入；关闭它相当于按 Ctrl-D。
    eval "exec $2>\"$work/$1.in\""
}

"${guard[@]}" "$bin_dir/chat_server" -p "$port" -v >"$work/server.log" 2>&1 &
server_pid=$!
pids+=("$server_pid")
wait_for "$work/server.log" "listening on"

echo "== register"
start_client alice 3 alice
wait_for "$work/alice.out" "* welcome to Mdr Chat Room, alice (1 online)"
start_client bob 4 bob
wait_for "$work/bob.out" "* welcome to Mdr Chat Room, bob (2 online)"
wait_for "$work/alice.out" "* bob joined"
# 第三个客户端也想叫 alice：被拒绝后换个名字
start_client carol 5 alice
wait_for "$work/carol.out" "! nickname is already in use: alice"
echo "/nick carol" >&5
wait_for "$work/carol.out" "* welcome to Mdr Chat Room, carol (3 online)"
wait_for "$work/alice.out" "* carol joined"

echo "== broadcast"
echo "hello, everyone" >&3
for who in alice bob carol; do
    wait_for "$work/$who.out" "<alice> hello, everyone"
done
echo "你好" >&4
wait_for "$work/carol.out" "<bob> 你好"

echo "== private message"
echo "/msg carol see you at 8" >&4
wait_for "$work/carol.out" "[private] <bob> see you at 8"
echo "/msg nobody hi" >&3
wait_for "$work/alice.out" "! no such user: nobody"

echo "== list and rename"
echo "/list" >&5
wait_for "$work/carol.out" "* online: alice bob carol"
echo "/nick robert" >&4
for who in alice bob carol; do
    wait_for "$work/$who.out" "* bob is now known as robert"
done

echo "== split and coalesced frames"
exec 6<>"/dev/tcp/127.0.0.1/$port"
# NICK "dave" = 01 0005 04 "dave"，一次只写一个字节，模拟最碎的拆包
for byte in '\x01' '\x00' '\x05' '\x04' d a v e; do
    printf "$byte" >&6
    sleep 0.05
done
wait_for "$work/alice.out" "* dave joined"
# 两帧 SAY 在同一次写入里发出，模拟粘包
printf '\x02\x00\x03\x00hi\x02\x00\x04\x00yo!' >&6
wait_for "$work/alice.out" "<dave> hi"
wait_for "$work/alice.out" "<dave> yo!"
exec 6>&-
wait_for "$work/alice.out" "* dave left"

echo "== malformed frame"
exec 6<>"/dev/tcp/127.0.0.1/$port"
printf '\x02\xff\xff' >&6 # body_len = 65535，超过上限
# 服务器应回一帧 ERROR，然后关闭连接，cat 读到文件结束就退出
"${guard[@]}" cat <&6 3>&- 4>&- 5>&- >"$work/raw.out"
exec 6>&-
grep -qa "bad frame length" "$work/raw.out" ||
    fail "no ERROR frame for a bad length"
wait_for "$work/server.log" "protocol error: bad frame length"
# 其他人不受影响
echo "/list" >&5
wait_for "$work/carol.out" "* online: alice robert carol"

echo "== piped input"
# 标准输入是管道：输入一结束客户端就半关闭，但仍能收到回复。
# 最后一行故意不带换行符，它也应当被发出。
printf 'from a pipe\n/list' |
    "${guard[@]}" "$bin_dir/chat_client" -p "$port" erin 3>&- 4>&- 5>&- 6>&- \
        >"$work/erin.out" 2>&1
for line in "<erin> from a pipe" "* online: alice robert carol erin" \
    "connection closed by server"; do
    wait_for "$work/erin.out" "$line"
done
wait_for "$work/carol.out" "* erin left"

echo "== privacy"
if grep -qF "see you at 8" "$work/alice.out"; then
    fail "alice saw a private message for carol" "$work/alice.out"
fi

echo "== quit"
echo "/quit" >&3
exec 3>&-
wait_for "$work/carol.out" "* alice left"
exec 4>&- # robert 的输入结束，相当于按 Ctrl-D
wait_for "$work/carol.out" "* robert left"
exec 5>&-

echo "== shutdown"
kill -TERM "$server_pid"
wait "$server_pid"
status=$?
[ "$status" -eq 0 ] || fail "server exited with status $status" "$work/server.log"
wait_for "$work/server.log" "server stopped"

echo "integration: all checks passed"
