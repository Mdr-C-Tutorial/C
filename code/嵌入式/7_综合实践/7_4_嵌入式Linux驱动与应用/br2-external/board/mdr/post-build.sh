#!/bin/sh
#
# Buildroot 生成映像之前调用，参数是 TARGET_DIR。
# 做覆盖层做不了的事：建空目录、建普通用户、删掉不该出现的东西。

set -eu

TARGET_DIR="$1"

# 配置与需要持久保存的数据
mkdir -p "$TARGET_DIR/data"
chmod 0755 "$TARGET_DIR/data"

# 运行期目录，打包时必须是空的
mkdir -p "$TARGET_DIR/var/run" "$TARGET_DIR/var/log"
chmod 0755 "$TARGET_DIR/var/run" "$TARGET_DIR/var/log"

# imud 以普通用户运行，设备节点的权限由 mdev 规则给
if ! grep -q '^imud:' "$TARGET_DIR/etc/passwd"; then
    echo 'imud:x:1000:1000:imud:/data:/bin/sh' >> "$TARGET_DIR/etc/passwd"
    echo 'imud:x:1000:' >> "$TARGET_DIR/etc/group"
fi

# 设备节点的属主与权限：/dev/mdr_imu 给 imud 用户，0600
if [ -f "$TARGET_DIR/etc/mdev.conf" ]; then
    grep -q 'mdr_imu' "$TARGET_DIR/etc/mdev.conf" || \
        echo 'mdr_imu 0:1000 0660' >> "$TARGET_DIR/etc/mdev.conf"
else
    echo 'mdr_imu 0:1000 0660' > "$TARGET_DIR/etc/mdev.conf"
fi

rm -rf "$TARGET_DIR/etc/dropbear"
