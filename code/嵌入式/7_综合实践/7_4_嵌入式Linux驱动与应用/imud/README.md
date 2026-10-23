# imud：IMU 服务

读取 `/dev/mdr_imu` 的定长样本，计算倾角，按周期把遥测发布到 MQTT 服务器；
命令主题接收 `rate=<Hz>`，经 sysfs 下发给驱动。

## 依赖

- coreMQTT 2.3.1。仓库里不带第三方源码，首次构建前取一份：

  ```sh
  ../scripts/get-deps.sh
  ```

  它会 clone 到 `third_party/coreMQTT`。版本固定为 v2.3.1，与第 7.3 节的固件一致。

## 构建

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DCOREMQTT_DIR="$PWD/third_party/coreMQTT"
cmake --build build -j2
```

CMake 会先找 sysroot 里有没有 `libcoreMQTT.a`（Buildroot 构建时的情况），
找不到才用 `COREMQTT_DIR` 指向的源码自己编。`-Wall -Wextra` 下应当无警告。

## 运行

```sh
./build/imud -f imud.conf
```

`-f` 指定配置文件，缺省读当前目录下的 `imud.conf`；`-s` 临时覆盖配置里的
数据源，调试时把它指向命名管道或普通文件。配置文件是 `key = value` 一行一项，
取值超出范围的项让整次加载失败，旧配置保留；`SIGHUP` 重新加载，`SIGTERM` /
`SIGINT` 干净退出。

| 项 | 默认值 | 说明 |
| --- | --- | --- |
| `source` | `/dev/mdr_imu` | 数据源；命名管道或普通文件也可以 |
| `host` | `127.0.0.1` | MQTT 服务器 |
| `port` | `1883` | MQTT 端口 |
| `device_id` | `mdr-dk1-01` | 客户端标识，也是主题里的设备号 |
| `topic_prefix` | `mdr` | 主题前缀 |
| `period_ms` | `1000` | 上报周期 |
| `sample_rate` | `100` | 采样率，用于没有时间戳时的 `dt` |
| `alpha` | `0.98` | 互补滤波系数，0～1 之间 |

数据源是普通文件时，追加数据就等于“有新样本”，50 ms 的采样定时器把它读走；
命名管道走事件循环的就绪通知；字符设备 `/dev/mdr_imu` 也走定时器读，
不管驱动是中断模式还是轮询模式，代码路径都一样。

## 测试

```sh
./build/test_imud
```

37 项检查，覆盖字节序往返、半个样本的拼接、输出数组满时把整样本留在缓冲里、
平放与倾斜时的姿态角、陀螺仪积分与加速度计修正的稳态、配置的正常与非法输入。
输出里有一行“配置错误: period_ms = 5 超出范围”，那是测试故意喂非法配置，
不是测试失败。

端到端的集成测试在 `../scripts/run_demo.sh`：普通文件作数据源，配一个只处理
CONNECT、SUBSCRIBE、QoS 0 PUBLISH、PINGREQ 的伪服务器（`../scripts/fake_broker.c`），
跑完两批样本后打印发布出去的内容。

## 文件

| 文件 | 内容 |
| --- | --- |
| `main.c` | 事件循环、定时器、信号处理、配置重载 |
| `config.[ch]` | 配置对象与 `key = value` 解析 |
| `filter.[ch]` | 互补滤波与倾角换算 |
| `sample_io.[ch]` | 字节流到定长样本的拼装，小端字节序的显式拆装 |
| `mqtt_glue.[ch]` | coreMQTT 的初始化、连接、发布、命令回调 |
| `transport_posix.c` | coreMQTT 的 POSIX 套接字传输 |
| `core_mqtt_config.h` | coreMQTT 的编译配置，库与应用的编译都用它 |
| `evl.[ch]` | 事件循环，复制自系统编程第 12 章 |
| `tests/test_imud.c` | 单元测试 |