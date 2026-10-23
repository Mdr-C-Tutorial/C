# Changelog

本文件记录 wordfreq 的每个版本对用户可见的变化，格式参照
[Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循[语义化版本](https://semver.org/lang/zh-CN/)。

## [Unreleased]

## [1.0.0] - 2026-10-21

### Added

- `wordfreq --version` 输出程序版本号。
- 公开头文件的接口文档，可用 Doxygen 生成参考文档。
- `include/wf/export.h`，共享库构建时导出公开函数。
- CMake 的安装规则与 CPack 打包。

### Changed

- 计数模块改为有序数组加二分查找，接口与输出不变；处理 10^6 词的输入比 0.2.0 快约十倍。

### Fixed

- 超长单词截断时多写一个字节的问题（0.2.0 引入）。

## [0.2.0] - 2026-10-20

### Changed

- 计数模块从线性查找改成有序数组加二分查找（`wf_counter` 的内部表示，接口不变）。
- 新增 `tools/gen_words.c` 与 `tools/bench.sh`，用于生成可重复的输入和记录测量结果。

## [0.1.0] - 2026-10-18

### Added

- `wordfreq [-n N] [FILE...]` 的首次发布：分词、计数、输出三个模块，手工构建脚本、
  Makefile、CMake 与 XMake 构建文件，单元测试与端到端测试。

[Unreleased]: https://example.com/wordfreq/compare/v1.0.0...main
[1.0.0]: https://example.com/wordfreq/releases/tag/v1.0.0
[0.2.0]: https://example.com/wordfreq/releases/tag/v0.2.0
[0.1.0]: https://example.com/wordfreq/releases/tag/v0.1.0
