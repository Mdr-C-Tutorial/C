# 第 17 章的演示仓库

本章的 `wordfreq` 快照在第 16 章基础上增加了 `.gitattributes` 与 `tools/check.sh`。
两个脚本都在工作目录里建仓库，所有 Git 配置用 `GIT_CONFIG_GLOBAL` 指向工作目录内的
文件，不改用户真实的全局配置；作者与提交时间固定，提交哈希可以复现。

## 搭建演示仓库

`setup_demo.sh` 建一个裸仓库 `server.git` 和 alice、bob 两个克隆：

```sh
sh setup_demo.sh demo
```

真实输出：

```text
demo ready in demo (server.git, alice, bob)
```

`bisect_demo.sh` 生成 bisect 演示用的仓库：15 个提交，第 6 个提交调用了尚未定义的
`normalize_newlines`（编译不过），第 9 个提交补上实现，第 11 个提交把 `token.c` 换成
第 11 章的缺陷版本，其余提交只改 README，末尾打上 `v0.2` 标签：

```sh
sh bisect_demo.sh bisect-demo
```

## 推送被拒

bob 先推送，alice 的提交随后被拒（Debian 13，Git 2.47.3）：

```text
To ../server.git
 ! [rejected]        main -> main (fetch first)
error: failed to push some refs to '../server.git'
hint: Updates were rejected because the remote contains work that you do not
hint: have locally. This is usually caused by another repository pushing to
hint: the same ref. If you want to integrate the remote changes, use
hint: 'git pull' before pushing again.
hint: See the 'Note about fast-forwards' in 'git push --help' for details.
```

`git fetch` 之后 `git status -sb` 显示 `ahead 1, behind 1`，再 `git pull --no-rebase`
产生一个合并提交，推送成功。

## 用 bisect 找回归

`tools/check.sh` 按退出状态说话：全部通过 `exit 0`，测试失败 `exit 1`，构建失败
`exit 125`（bisect 跳过无法编译的提交）。它自己生成一个不含 `getline` 的
`build-bisect/include/wf/config.h`，不走 CMake，因此在任何平台上都能编译。

```sh
git bisect start HEAD v0.2
git bisect run sh tools/check.sh
```

真实输出（节选，Debian 13）：

```text
Bisecting: 6 revisions left to test after this (roughly 3 steps)
[f5e3f7f7b8aafd7823bf3a6fa6603ac203e276fe] chore: tidy 6
running 'sh' 'tools/check.sh'
app/main.c: In function ‘main’:
app/main.c:153:9: error: implicit declaration of function ‘normalize_newlines’ [-Wimplicit-function-declaration]
...
42691162cbeef44ef55d4a4de479819b1246f497 is the first bad commit
commit 42691162cbeef44ef55d4a4de479819b1246f497
    refactor: lowercase loop
 src/token.c | 20 ++++++++++++--------
 1 file changed, 12 insertions(+), 8 deletions(-)
bisect found first bad commit
```

## 换行符与 .gitattributes

`.gitattributes` 规定文本文件在索引里用 LF、检出的换行按平台转换，并强制 `*.sh` 使用 LF：

```text
* text=auto
*.sh text eol=lf
*.bat text eol=crlf
*.png binary
```

一个 CRLF 换行的脚本在 Linux 上执行（Debian 13，`/bin/sh` 是 dash）：

```text
$ bash tools_check.sh
tools_check.sh: line 3: set: -: invalid option
set: usage: set [-abefhkmnptuvxBCEHPT] [-o option-name] [--] [-] [arg ...]
tools_check.sh: line 4: cd: $'.\r': No such file or directory
...
$ sh tools_check.sh
tools_check.sh: 3: set: Illegal option -
```

`git add --renormalize .` 把已提交的文件按新属性重新规范化，`git ls-files --eol`
列出每个文件在索引与工作区中的换行符。

## 缺陷

本章没有另设注入缺陷：bisect 演示复用第 11 章的
`code/开发与工程基础/11_自动化测试/bugs/token_lowercase_starts_at_1.c`。