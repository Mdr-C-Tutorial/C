#!/bin/sh
# 取 coreMQTT 2.3.1 到 imud/third_party/coreMQTT。
# 第三方源码不提交仓库，需要时跑一次；版本固定，构建结果可复现。

set -eu

VERSION=v2.3.1
HERE=$(cd "$(dirname "$0")/.." && pwd)
DEST="$HERE/imud/third_party"

mkdir -p "$DEST"
if [ -d "$DEST/coreMQTT" ]; then
    echo "coreMQTT 已存在：$DEST/coreMQTT"
    exit 0
fi

git clone --depth 1 --branch "$VERSION" \
    https://github.com/FreeRTOS/coreMQTT.git "$DEST/coreMQTT"
cd "$DEST/coreMQTT"
git log -1 --format='coreMQTT %H %s'
