# 5.4 的配套代码

本目录是 5.4 页面用到的两个构建系统定制：Buildroot 的 `BR2_EXTERNAL` 外部树与 Yocto 的 `meta-mdr` 层。

## br2-external

外部树，配合 Buildroot 2026.02 LTS 使用。它不是独立工程，需要放在 Buildroot 源码树之外，构建时用 `BR2_EXTERNAL` 指过去。

```text
br2-external/
├── external.desc
├── external.mk
├── Config.in
├── configs/
│   └── mdr_qemu_defconfig
├── board/mdr/
│   ├── post-build.sh
│   └── rootfs-overlay/
└── package/
    ├── sensord/       # 5.10 的用户态服务
    └── mdr-fifo/      # 5.7 的内核模块
```

取得 Buildroot 并构建：

```console
$ wget https://buildroot.org/downloads/buildroot-2026.02.tar.gz
$ tar xf buildroot-2026.02.tar.gz
$ make -C buildroot-2026.02 O=output \
      BR2_EXTERNAL=$PWD/br2-external mdr_qemu_defconfig
$ make -C buildroot-2026.02 O=output
```

首次构建要下载内核、BusyBox 与工具链，视网络情况在一小时上下。产物在 `output/images/`：

- `zImage`：内核；
- `vexpress-v2p-ca9.dtb`：设备树；
- `rootfs.ext2`：根文件系统。

启动：

```console
$ qemu-system-arm -M vexpress-a9 -m 256M \
      -kernel output/images/zImage \
      -dtb output/images/vexpress-v2p-ca9.dtb \
      -drive file=output/images/rootfs.ext2,format=raw,if=sd \
      -append "root=/dev/mmcblk0 rw console=ttyAMA0" \
      -nographic
```

`sensord` 的源码与配置来自 `code/嵌入式/5_嵌入式Linux/5_10_用户空间服务_部署与调试/`，`mdr_fifo.c` 来自 5.7 的示例。两处都是复制过来的副本，改的时候要两边同步。

模块的源代码由 `package/mdr-fifo/src/` 提供，`sensord` 的由 `package/sensord/src/` 提供，均用 `SITE_METHOD = local`，不需要网络。

## meta-mdr

Yocto 层，兼容 5.0 Scarthgap 与 6.0 Wrynose。它不是独立工程，需要配合 Poky 使用。

```text
meta-mdr/
├── conf/layer.conf
├── recipes-apps/sensord/
│   ├── sensord_1.0.bb
│   └── files/            # 配方引用的源码与配置
└── recipes-kernel/mdr-fifo/
    ├── mdr-fifo_1.0.bb
    └── files/
```

放入 Poky 的构建环境：

```console
$ git clone -b wrynose git://git.yoctoproject.org/poky
$ cd poky
$ source oe-init-build-env build
$ echo 'BBLAYERS += "/path/to/meta-mdr"' >> conf/bblayers.conf
$ echo 'IMAGE_INSTALL:append = " sensord mdr-fifo"' >> conf/local.conf
$ bitbake core-image-minimal
```

两个配方都用 `file://` 从本层的 `files/` 取源码，`S = "${UNPACKDIR}"` 指向 BitBake 解包后的目录。Yocto 5.1 起不再允许 `S = "${WORKDIR}"`，老教程里的写法照抄会报错。

本仓库不提交第三方源码；Buildroot 与 Poky 都要按页面里的版本号自行获取。