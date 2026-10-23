#!/bin/sh
#
# mkrootfs.sh：用 BusyBox 拼一个最小根文件系统。
#
#   ./mkrootfs.sh <busybox 源码目录> <目标目录> <交叉前缀> [sysroot]
#
# 例：
#   ./mkrootfs.sh ~/src/busybox-1.37.0 ~/rootfs arm-linux-gnueabihf- \
#       ~/buildroot/output/staging
#
# 目标目录必须不存在或为空。第四个参数省略时不拷 C 库，
# 适合 BusyBox 静态编译的情况。

set -eu

BUSYBOX_DIR="${1:?用法: mkrootfs.sh <busybox 源码目录> <目标目录> <交叉前缀> [sysroot]}"
ROOTFS="${2:?缺少目标目录}"
CROSS="${3:?缺少交叉前缀}"
SYSROOT="${4:-}"

if [ -d "$ROOTFS" ] && [ -n "$(ls -A "$ROOTFS")" ]; then
    echo "$ROOTFS 不为空，先清理再重试" >&2
    exit 1
fi

mkdir -p "$ROOTFS"/{bin,sbin,etc/init.d,proc,sys,dev,lib,usr/bin,usr/sbin}

# 1. 构建并安装 BusyBox。配置里打开静态链接或者指向 sysroot，
#    二者选一，脚本不替你决定。
make -C "$BUSYBOX_DIR" ARCH=arm CROSS_COMPILE="$CROSS" -j"$(nproc)"
make -C "$BUSYBOX_DIR" ARCH=arm CROSS_COMPILE="$CROSS" \
    CONFIG_PREFIX="$ROOTFS" install

# 2. 动态链接时补齐动态链接器与 C 库。
if [ -n "$SYSROOT" ]; then
    for lib in ld-linux-armhf.so.3 libc.so.6; do
        if [ -e "$SYSROOT/lib/$lib" ]; then
            cp -a "$SYSROOT/lib/$lib" "$ROOTFS/lib/"
        else
            echo "警告: $SYSROOT/lib/$lib 不存在" >&2
        fi
    done
fi

# 3. 启动脚本。devtmpfs 让内核自动生成 /dev 下的节点。
cat > "$ROOTFS/etc/init.d/rcS" <<'EOF'
#!/bin/sh

mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev

echo "mdr rootfs ready"
EOF
chmod +x "$ROOTFS/etc/init.d/rcS"

cat > "$ROOTFS/etc/inittab" <<'EOF'
::sysinit:/etc/init.d/rcS
::askfirst:-/bin/sh
::ctrlaltdel:/sbin/reboot
::shutdown:/bin/umount -a -r
EOF

# 4. 打包成 initramfs。cpio 必须在根目录里执行，
#    否则归档里的路径会带上目标目录的名字。
(
    cd "$ROOTFS"
    find . | cpio -o -H newc | gzip -9
) > "$ROOTFS.cpio.gz"

ls -l "$ROOTFS.cpio.gz"