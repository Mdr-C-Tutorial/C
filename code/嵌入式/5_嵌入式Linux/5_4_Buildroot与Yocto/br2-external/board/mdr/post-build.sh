#!/bin/sh
#
# Buildroot 在生成映像之前调用本脚本，参数是 TARGET_DIR。
# 这里做覆盖层做不了的事：建空目录、设所有者、删掉不需要的东西。

set -eu

TARGET_DIR="$1"

# sensord 的配置文件与数据放在 /data，日志走 stderr 由 init 收集。
mkdir -p "$TARGET_DIR/data"
chmod 0755 "$TARGET_DIR/data"

# 运行期目录，rootfs 打包时必须是空的。
mkdir -p "$TARGET_DIR/var/run" "$TARGET_DIR/var/log"
chmod 0755 "$TARGET_DIR/var/run" "$TARGET_DIR/var/log"

# 调试用的 root 登录已经由 defconfig 允许，这里只确认没有残留的 ssh 主机密钥。
rm -rf "$TARGET_DIR/etc/dropbear"
