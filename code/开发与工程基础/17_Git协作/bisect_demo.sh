#!/bin/sh
# 为第 17 章的 bisect 演示生成一个仓库：15 个提交。
#
# 第 6 个提交调用了尚未定义的 normalize_newlines（编译不过），
# 第 9 个提交补上实现；第 11 个提交把 token.c 换成第 11 章缺陷版本
# （"The" 不被转成小写）。其余提交只改 README。
#
#   sh bisect_demo.sh [目录]
#
# 默认建在 ./bisect-demo；所有 Git 配置写在工作目录内。
set -eu

BASE=${1:-bisect-demo}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

rm -rf "$BASE"
mkdir -p "$BASE"

export GIT_CONFIG_GLOBAL="$BASE/gitconfig"
printf '[user]\n\tname = demo\n\temail = demo@example.com\n[init]\n\tdefaultBranch = main\n[core]\n\tautocrlf = false\n' \
    > "$GIT_CONFIG_GLOBAL"

cp -r "$HERE/wordfreq/." "$BASE/"
rm -rf "$BASE/build" "$BASE/build-bisect" "$BASE/build-check" "$BASE/.git"

cd "$BASE"
git init -q
git add -A
GIT_AUTHOR_DATE='2026-10-15T09:00:00+0800' GIT_COMMITTER_DATE='2026-10-15T09:00:00+0800' \
    git commit -qm "wordfreq v0.2"
git tag v0.2

# 第 2–9 个提交：大部分是无关的小改动，
# 其中第 6 个引入一次编译失败，第 9 个补上
n=0
tidy() {
    n=$((n + 1))
    printf '<!-- tidy %s -->\n' "$n" >> README.md
    GIT_AUTHOR_DATE="2026-10-16T09:$n:00+0800" GIT_COMMITTER_DATE="2026-10-16T09:$n:00+0800" \
        git commit -qam "chore: tidy $n"
}

tidy
tidy
tidy
python3 - <<'PY'
import io

p = 'app/main.c'
s = io.open(p, encoding='utf-8', newline='').read()
old = '    if (argi == argc) {'
new = '    if (normalize_newlines() != 0) {\n        return 1;\n    }\n    if (argi == argc) {'
assert old in s
io.open(p, 'w', encoding='utf-8', newline='').write(s.replace(old, new, 1))
PY
GIT_AUTHOR_DATE='2026-10-16T10:00:00+0800' GIT_COMMITTER_DATE='2026-10-16T10:00:00+0800' \
    git commit -qam "wip: 在 main 里调用换行规范化"
tidy
tidy
tidy
python3 - <<'PY'
import io

p = 'app/main.c'
s = io.open(p, encoding='utf-8', newline='').read()
old = 'static int count_file(wf_counter *counter, const char *path) {'
new = 'static int normalize_newlines(void) { return 0; }\n\nstatic int count_file(wf_counter *counter, const char *path) {'
assert old in s
io.open(p, 'w', encoding='utf-8', newline='').write(s.replace(old, new, 1))
PY
GIT_AUTHOR_DATE='2026-10-16T10:10:00+0800' GIT_COMMITTER_DATE='2026-10-16T10:10:00+0800' \
    git commit -qam "feat: 补上换行规范化函数"

# 第 11 个提交：引入第 11 章的缺陷
tidy
cp "$HERE/../11_自动化测试/bugs/token_lowercase_starts_at_1.c" src/token.c
GIT_AUTHOR_DATE='2026-10-17T09:00:00+0800' GIT_COMMITTER_DATE='2026-10-17T09:00:00+0800' \
    git commit -qam "refactor: lowercase loop"

# 第 12–15 个提交：继续无关改动
tidy
tidy
tidy
tidy

echo "bisect demo ready in $BASE"
git log --oneline