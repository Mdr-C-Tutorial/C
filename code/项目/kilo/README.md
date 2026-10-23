# 2.2 kilo：配套代码

正文里用到的程序和补丁都在这里。kilo 本身不在仓库里，按正文第 1 节克隆并检出提交 `323d93b`，再把本目录的文件放到它旁边，构成一个平铺的目录：

```text
kilo/
├── kilo.c          # 上游源码
├── Makefile        # 本目录的 Makefile（编译 kilo 与两个程序）
├── kilo_drive.c
├── tty_probe.c
├── tests/
├── tests-ln/
└── patches/
```

| 文件 | 正文位置 | 说明 |
| --- | --- | --- |
| `kilo_drive.c` | 4 | 伪终端驱动程序：按脚本向 kilo 送入按键，收集屏幕输出 |
| `tty_probe.c` | 2 | 观察 kilo 正常退出与被信号杀死之后，伪终端是否还停在 raw 模式 |
| `tests/*.keys` | 4.4 | 按键脚本，一行一条动作 |
| `tests/*.start` | 4.4 | 打开编辑器时文件的初始内容 |
| `tests/*.expect` | 4.4 | 期望的屏幕末帧（去掉转义序列后） |
| `tests/short.*` | 5.3 | 两字节的 C 文件：普通构建上无意义，在 ASan 构建上当 #36 的哨兵 |
| `tests-ln/*` | 7 | 加了行号之后同一批用例的期望，`make test-ln` 使用 |
| `patches/0001-keyword-memcmp.patch` | 5 | issue #36：关键字比较越界读 |
| `patches/0002-row-index-after-delete.patch` | 6 | issue #117：删行后 `idx` 方向错误 |
| `patches/0003-line-numbers.patch` | 7 | 显示行号 |

用法：

```text
make                     编译 kilo、kilo_drive 与 tty_probe
make test KILO=./kilo    用 tests/ 跑回归测试
make test-ln KILO=./kilo_ln
```

`KILO` 指向已经构建好的 kilo。用例的输入文件在工作目录里复制成 `NAME.c` 再交给 kilo，因为扩展名决定是否启用 C 的语法高亮；没有高亮，删行用例就碰不到 `editorUpdateSyntax`，也就测不出 #117。`tty_probe` 要在 kilo 所在目录下运行，它默认启动 `./kilo t.c`。

补丁在 kilo 仓库里用 `git apply` 或 `patch -p1` 应用，两者都接受仓库检出的 CRLF 工作副本：

```text
git apply patches/0001-keyword-memcmp.patch
```

全部输出都在 Debian 13（gcc 14.2、glibc 2.41）上实测。