#!/bin/sh
# 搭建第 17 章演示用的三个仓库：一个裸仓库充当服务器，alice 与 bob 各克隆一份。
#
#   sh setup_demo.sh [目录]
#
# 默认建在 ./demo 下；所有 Git 配置写在工作目录内，不改用户的全局配置。
# 初始提交的作者与时间固定，因此提交哈希可以复现。
set -eu

BASE=${1:-demo}
SRC=$(CDPATH= cd -- "$(dirname -- "$0")/wordfreq" && pwd)

rm -rf "$BASE"
mkdir -p "$BASE"

export GIT_CONFIG_GLOBAL="$BASE/gitconfig"
printf '[user]\n\tname = demo\n\temail = demo@example.com\n[init]\n\tdefaultBranch = main\n[core]\n\tautocrlf = false\n' \
    > "$GIT_CONFIG_GLOBAL"
export GIT_AUTHOR_NAME=alice
export GIT_AUTHOR_EMAIL=alice@example.com
export GIT_COMMITTER_NAME=alice
export GIT_COMMITTER_EMAIL=alice@example.com
export GIT_AUTHOR_DATE='2026-10-19T09:00:00+0800'
export GIT_COMMITTER_DATE='2026-10-19T09:00:00+0800'

# 裸仓库的默认分支设为 main，克隆时才能检出内容
git init -q --bare --initial-branch=main "$BASE/server.git"

# 从快照建立初始提交，再克隆出两个工作副本
mkdir "$BASE/seed"
cp -r "$SRC/." "$BASE/seed/"
rm -rf "$BASE/seed/build" "$BASE/seed/build-bisect" "$BASE/seed/.git"
(
    cd "$BASE/seed"
    git init -q
    git checkout -q -b main
    git add -A
    git commit -qm "wordfreq v0.2"
    git remote add origin ../server.git
    git push -q origin main
)
rm -rf "$BASE/seed"

git clone -q "$BASE/server.git" "$BASE/alice"
git clone -q "$BASE/server.git" "$BASE/bob"

# 克隆里记录的 URL 是相对路径，脚本挪到别处也能用
for who in alice bob; do
    git -C "$BASE/$who" remote set-url origin ../server.git
    git -C "$BASE/$who" config user.name "$who"
    git -C "$BASE/$who" config user.email "$who@example.com"
done

echo "demo ready in $BASE (server.git, alice, bob)"