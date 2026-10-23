# 第 19 章的演示材料

本章没有注入缺陷。`wordfreq` 升到 v1.0.0：公开头文件改为 Doxygen 注释，
版本号只在 `project(VERSION 1.0.0)` 定义一次并由 `configure_file` 生成 `version.h`，
新增 `--version`、`CHANGELOG.md`、`LICENSE`、`Doxyfile`、安装规则与 CPack 打包。

`examples/abi_struct/` 与 `examples/abi_enum/` 是 4.1 节的两组最小实验：
同一份库按 v1.0.0 与 v1.1.0 两版头文件编译，旧程序链接新库，看 ABI 破坏的后果。

## Doxygen

```sh
doxygen Doxyfile
```

`Doxyfile` 里打开了 `WARN_IF_UNDOCUMENTED`、`WARN_IF_INCOMPLETE_DOC` 与 `WARN_NO_PARAMDOC`。
把 `wf_counter_top` 注释里的 `@param n` 删掉一行，实际报出：

```text
D:/work/wordfreq/include/wf/counter.h:65: warning: The following parameter of wf_counter_top(const wf_counter *c, wf_entry *out, size_t n) is not documented:
  parameter 'n'
Doxygen version used: 1.10.0 (GIT-NOTFOUND)
```

补回后 `doxygen Doxyfile` 无警告，`docs/html/index.html` 生成。

CMake 里 Doxygen 是可选的：只有 `-DWF_BUILD_DOCS=ON` 时才 `find_package(Doxygen)` 并
生成 `docs` 目标。该目标不读仓库里的 `Doxyfile`，它的设置在配置时用
`-DDOXYGEN_<选项名>=...` 传入。

## 版本号

```text
$ ./build/wordfreq --version
wordfreq 1.0.0
```

源码里没有第二处写死的 `1.0.0`；把 `project(wordfreq VERSION 1.1.0 ...)` 改掉重新配置，
`--version` 的输出随之改变（`configure_file` 在配置阶段重新生成 `version.h`）。
第 8 章的 `Makefile` 用 `sed` 从 `CMakeLists.txt` 读同一个版本号，两条构建路径共用一个来源。

## 打标签与源码包

```sh
git tag -a v1.0.0 -m "wordfreq 1.0.0"
git show --no-patch v1.0.0
git archive --format=tar.gz --prefix=wordfreq-1.0.0/ -o wordfreq-1.0.0.tar.gz v1.0.0
```

`git show --no-patch v1.0.0` 的真实输出开头：

```text
tag v1.0.0
Tagger: Mdr <mdr@example.com>
Date:   Thu Oct 22 01:04:14 2026 +0800

wordfreq 1.0.0

commit e9f24bdb2685a7fb365224114606e3c6d6afe089
```

源码包共 51 个条目，连续导出两次的 SHA-256 相同。

源码包解压到空目录后可以直接构建并通过测试：

```sh
mkdir clean && tar xzf wordfreq-1.0.0.tar.gz -C clean
cd clean/wordfreq-1.0.0
cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build
```

## 二进制包与校验值

```sh
cmake --install build --prefix "$PWD/prefix"
cpack --config build/CPackConfig.cmake -G ZIP
```

Windows 上生成 `wordfreq-1.0.0-win64.zip`；Linux（SignalX）上 `-G TGZ` 生成
`wordfreq-1.0.0-Linux.tar.gz`。ZIP 包共 18 个条目。

校验值：

```text
$ sha256sum wordfreq-1.0.0-Linux.tar.gz
b222274929adbc3b660f3ea3f1473bbd9bf543e04c1aa64aa7abbae8320a3a81  wordfreq-1.0.0-Linux.tar.gz
```

```text
PS> Get-FileHash -Algorithm SHA256 .\wordfreq-1.0.0-win64.zip

Algorithm : SHA256
Hash      : 66AC228F9D324F38362F3225CE841036CB9B73FC30255E16CCC344A813BB4272
Path      : D:\work\wordfreq\wordfreq-1.0.0-win64.zip
```

两个包的内容不同（平台、时间戳），校验值自然不同。同一台机器上重新打包也会因为时间戳
变化而得到不同的值（实测连续两次分别是 `be3695d8…` 与 `66ac228f…`），这是预期行为。
`git archive` 的源码包则逐字节可复现，同一标签连续导出两次都是 `d810d245…`。
