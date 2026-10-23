#!/bin/sh
# 本地与 CI 共用的检查脚本：配置、构建、运行全部测试。
#
#   sh tools/ci.sh [preset]
#
# preset 省略时用 dev-gcc（Ninja）。没有 Ninja 的环境用 dev-gcc-make
# （Unix Makefiles），两条路径执行的是同一张构建图。
# CI 的每个作业都调用这个脚本或同样的预设命令，不在 YAML 里另外堆一套构建命令。
set -eu

PRESET=${1:-dev-gcc}

cd "$(dirname "$0")/.."

echo "== configure ($PRESET)"
cmake --preset "$PRESET"

echo "== build ($PRESET)"
cmake --build --preset "$PRESET"

echo "== test ($PRESET)"
ctest --preset "$PRESET"
