#!/bin/sh
# 重建《4. Git 本地工作流》正文里的 greet 演示仓库。
# 用法：sh demo.sh [目标目录]（默认 ./greet-demo）。目标目录必须不存在或为空。
# 作者、日期与提交信息都固定，因此生成的提交哈希与正文中的一致；
# 脚本不读取也不修改任何全局 Git 配置。
set -eu

TARGET=${1:-./greet-demo}

if [ -e "$TARGET" ] && [ -n "$(ls -A "$TARGET" 2>/dev/null)" ]; then
    echo "demo.sh: 目标目录已存在且非空：$TARGET" >&2
    exit 1
fi

export GIT_AUTHOR_NAME=Mdr
export GIT_AUTHOR_EMAIL=mdr@example.com
export GIT_COMMITTER_NAME=Mdr
export GIT_COMMITTER_EMAIL=mdr@example.com

mkdir -p "$TARGET"
cd "$TARGET"

commit() {
    date=$1
    shift
    GIT_AUTHOR_DATE=$date GIT_COMMITTER_DATE=$date git commit --quiet "$@"
}

git init --quiet --initial-branch=main
# 换行符按工作区原样入库，避免 Windows 上的 core.autocrlf 改变文件内容与提交哈希。
git config core.autocrlf false
# 签名提交会改变提交对象，关掉才能复现正文里的哈希。
git config commit.gpgsign false

cat > greet.c <<'EOF'
#include <stdio.h>

int main(void) {
    puts("Hello, world!");
    return 0;
}
EOF
cat > README.md <<'EOF'
# greet

打印一句问候。
EOF
cat > build.sh <<'EOF'
#!/bin/sh
set -eu
mkdir -p build
gcc -std=c17 -Wall -Wextra -o build/greet greet.c
EOF

git add greet.c README.md build.sh
commit "2026-10-19T09:00:00+08:00" -m "feat: 打印问候语" \
    -m "先输出一句固定的问候，下一步再接受命令行参数。"

cat > .gitignore <<'EOF'
build/
*.o
*.exe
a.out
*.gcda
.vscode/
EOF
git add .gitignore
commit "2026-10-19T09:20:00+08:00" -m "build: 忽略构建产物"

cat > greet.c <<'EOF'
#include <stdio.h>

int main(int argc, char *argv[]) {
    const char *name = "World";
    if (argc > 1) {
        name = argv[1];
    }
    printf("Hello, %s!\n", name);
    return 0;
}
EOF
git add greet.c
commit "2026-10-19T09:40:00+08:00" -m "feat: 支持从命令行读入名字。" \
    -m "参数缺省时仍然问候 world。"

# 先提交一条说明末尾带句号的版本，再用 amend 改掉，对应正文第 6 节。正文的提交哈希以改后为准。
GIT_AUTHOR_DATE="2026-10-19T09:40:00+08:00" \
GIT_COMMITTER_DATE="2026-10-19T09:40:00+08:00" \
    git commit --quiet --amend -m "feat: 支持从命令行读入名字" \
        -m "参数缺省时仍然问候 world。"

sed -i 's/const char \*name = "World";/const char *name = "Mdr";/' greet.c
git add greet.c
commit "2026-10-19T10:00:00+08:00" -m "feat: 默认问候 Mdr" \
    -m "本机自用时省去输入名字。"

GIT_AUTHOR_DATE="2026-10-19T10:10:00+08:00" \
GIT_COMMITTER_DATE="2026-10-19T10:10:00+08:00" \
    git revert --quiet --no-edit HEAD

cat > README.md <<'EOF'
# greet

打印一句问候。

## 用法

    sh build.sh
    ./build/greet Ada
EOF
git add README.md
commit "2026-10-19T10:20:00+08:00" -m "docs: 在 README 中补充用法"

cat > greet.c <<'EOF'
#include <stdio.h>
#include <string.h>

static void print_usage(void) {
    puts("usage: greet [-h] [NAME]");
}

int main(int argc, char *argv[]) {
    const char *name = "world";
    if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        print_usage();
        return 0;
    }
    if (argc > 1) {
        name = argv[1];
    }
    printf("Hello, %s!\n", name);
    return 0;
}
EOF
git add greet.c
commit "2026-10-19T10:30:00+08:00" -m "feat: 增加 -h 用法提示" \
    -m "用法信息交给 print_usage 输出。"

git switch --quiet --create shout
sed -i 's/printf("Hello, %s!\\n", name);/printf("HELLO, %s!\\n", name);/' greet.c
git add greet.c
commit "2026-10-19T10:40:00+08:00" -m "feat: 问候语改为大写"
git switch --quiet main
git merge --quiet shout

git switch --quiet --create polite
sed -i 's/printf("HELLO, %s!\\n", name);/printf("hello, %s!\\n", name);/' greet.c
git add greet.c
commit "2026-10-19T10:50:00+08:00" -m "fix: 问候语改用小写"

git switch --quiet main
sed -i 's/printf("HELLO, %s!\\n", name);/printf("HELLO, %s!!!\\n", name);/' greet.c
git add greet.c
commit "2026-10-19T11:00:00+08:00" -m "feat: 问候语末尾加感叹号"

git merge polite || true

cat > greet.c <<'EOF'
#include <stdio.h>
#include <string.h>

static void print_usage(void) {
    puts("usage: greet [-h] [NAME]");
}

int main(int argc, char *argv[]) {
    const char *name = "world";
    if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        print_usage();
        return 0;
    }
    if (argc > 1) {
        name = argv[1];
    }
    printf("hello, %s!!!\n", name);
    return 0;
}
EOF
git add greet.c
GIT_AUTHOR_DATE="2026-10-19T11:10:00+08:00" \
GIT_COMMITTER_DATE="2026-10-19T11:10:00+08:00" \
    git commit --quiet -m "chore: 合并 polite 分支" \
        -m "两处改动都保留：小写的 hello 和三个感叹号。"

git branch --quiet --delete polite

sh build.sh
echo "提交历史："
git log --graph --oneline --all
echo
echo "运行结果："
./build/greet
./build/greet Ada
./build/greet -h