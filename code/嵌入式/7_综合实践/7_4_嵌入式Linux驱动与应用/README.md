# 7.4 嵌入式 Linux 驱动与应用：配套代码

一个完整的小产品：MPU6050 的 Linux 字符设备驱动 + 用户态服务 imud +
Buildroot 外部树。正文里的代码片段都来自这些文件。

```text
driver/                     内核模块与 uapi 头
  mdr_mpu6050.c             platform/I2C/字符设备/线程化中断
  mdr_imu.h                 uapi 头：/dev/mdr_imu 的样本结构
  Makefile、Kbuild          kernel-module 与手工交叉编译共用
imud/                       用户态服务
  imud.h  config.[ch]       配置对象与 key = value 解析
  filter.[ch]               互补滤波与倾角换算
  sample_io.[ch]            字节流 → 定长样本的拼装
  mqtt_glue.[ch]            coreMQTT 的初始化、连接、发布
  transport_posix.c         coreMQTT 的 POSIX 套接字传输
  evl.[ch]                  事件循环（复制自系统编程第 12 章）
  core_mqtt_config.h        coreMQTT 的编译配置
  tests/test_imud.c         单元测试
br2-external/               Buildroot 外部树
  package/coremqtt/         coreMQTT 的包
  package/mdr-imu/          内核模块包
  package/imud/             imud 的包与出厂配置、启动脚本
  board/mdr/                设备树覆盖层、内核配置片段、post-build 脚本
  configs/                  两份 defconfig
scripts/
  get-deps.sh               取 coreMQTT 2.3.1
  fake_broker.c             只够联调用的 MQTT 服务器
  make_samples.py           生成 24 字节一个的样本流
  run_demo.sh               样本文件 + 伪 broker 的集成测试
  stub_smoke_test.sh        QEMU + i2c-stub 上的驱动冒烟测试
```

## 在宿主机上构建与测试

coreMQTT 不提交仓库，先取一份：

```sh
./scripts/get-deps.sh
```

然后构建与运行单元测试：

```sh
cmake -S imud -B imud/build -DCMAKE_BUILD_TYPE=Release \
      -DCOREMQTT_DIR="$PWD/imud/third_party/coreMQTT"
cmake --build imud/build -j2
imud/build/test_imud
```

伪服务器单独编译，再做集成测试（只绑 127.0.0.1，端口 25765）：

```sh
cc -std=c17 -Wall -Wextra -O2 -o scripts/fake_broker scripts/fake_broker.c
./scripts/run_demo.sh
```

## 交叉编译与打包

驱动与 imud 都通过 Buildroot 外部树构建，见正文第 4 节：

```sh
make BR2_EXTERNAL="$PWD/br2-external" mdr_qemu_defconfig
make
```

`mdr_stm32mp157_defconfig` 对应 DK 板。驱动模块交叉编译时需要目标内核
源码树先跑过 `make modules_prepare`，Buildroot 会自动处理。
