# wordfreq

统计文本中出现次数最多的单词。

## 用法

```text
usage: wordfreq [-n N] [FILE...]
       wordfreq --version
```

1. 输出出现次数最多的 N 个单词，N 默认为 10，可取 1 到 1000000。
2. 没有 FILE 时读标准输入；给出多个 FILE 时把它们合并统计。
3. 单词是连续的 ASCII 字母（A–Z、a–z），统计时转成小写；其他字节（数字、标点、空白、
   0x80 以上的字节）都是分隔符。超过 63 个字母的单词只保留前 63 个。
4. 按次数从多到少输出，次数相同的按 `strcmp` 升序排列。每行先是右对齐、占 7 列的次数，
   再是一个空格和单词。
5. 退出状态：0 表示成功；1 表示文件无法打开或读取（标准错误输出
   `wordfreq: <路径>: <错误说明>`），或内存不足；2 表示用法错误。

```text
$ wordfreq -n 3 tests/data/sample.txt
      4 the
      2 on
      2 sat
```

```text
$ wordfreq --version
wordfreq 1.0.0
```

## 构建

需要 CMake 3.20 或更新版本、支持 C17 的编译器，以及 Ninja 或 GNU Make。
在 bash 类 shell（Windows 上为 MSYS2 UCRT64 或 Git Bash）中：

```sh
cmake --preset dev-gcc
cmake --build --preset dev-gcc
ctest --preset dev-gcc
```

没有 Ninja 时用 `dev-gcc-make`（Unix Makefiles）。`tools/ci.sh` 把这三步合成一条命令，
CI 与本地跑的是同一套预设：

```sh
sh tools/ci.sh dev-gcc
```

可用的 CMake 选项：`WF_SANITIZE`（ASan + UBSan）、`WF_COVERAGE`（gcov）、`WF_FUZZ`
（libFuzzer 重放程序）、`WF_WERROR`（警告当错误）、`WF_HAVE_GETLINE`（读文件路径）、
`WF_BUILD_DOCS`（Doxygen 文档）。

## 测试

```sh
ctest --preset dev-gcc
```

`test_token` 与 `test_counter` 是单元测试，`e2e` 比较样例输出，`replay` 重放模糊测试的
语料与回归输入（需要 `-DWF_FUZZ=ON`）。

## 文档

公开头文件用 Doxygen 注释写接口文档。配置时打开 `WF_BUILD_DOCS` 后构建 `docs` 目标，
或者直接用仓库里的 `Doxyfile`：

```sh
doxygen Doxyfile
```

生成结果在 `docs/html/`。

## 安装与打包

```sh
cmake --install build --prefix /path/to/prefix
cpack --config build/CPackConfig.cmake -G ZIP
```

`install` 规则安装库、程序、公开头文件（含生成的 `config.h` 与 `version.h`）以及
README、CHANGELOG、LICENSE；CPack 生成 `wordfreq-1.0.0-<平台>.zip`。

## 许可证

MIT，见 `LICENSE`。源文件头部使用 SPDX 标识：`/* SPDX-License-Identifier: MIT */`。

## 目录

```text
wordfreq/
├── include/wf/       token.h、counter.h、report.h、export.h、version.h.in
├── src/              token.c、counter.c、report.c、version.c；config.h.in
├── app/main.c        命令行程序
├── tests/            单元测试、端到端测试与回归输入
├── fuzz/             模糊测试目标与重放程序
├── tools/            输入生成器、基准脚本、CI 脚本、bisect 检查脚本
├── Doxyfile          Doxygen 配置
├── CHANGELOG.md      更新日志（Keep a Changelog）
├── LICENSE           MIT 许可证
├── CMakePresets.json 配置、构建、测试与工作流预设
├── CMakeLists.txt    目标、测试、安装与打包
└── Makefile          第 8 章的构建文件
```
