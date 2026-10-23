# 7.3 联网固件

本目录是教程 7.3 节的配套程序：MQTT 报文的构造、回环联调，以及退避策略的比较。全部在主机上运行，不需要硬件。

## 主机程序

| 文件 | 内容 |
| --- | --- |
| `mqtt_packet.c/h` | MQTT 3.1.1 的 CONNECT、SUBSCRIBE、PUBLISH（QoS 0）、PINGREQ、DISCONNECT 构造器 |
| `mqtt_demo.c` | 同一进程里的伪服务器（127.0.0.1:25731）与设备端，打印每个报文 |
| `backoff_demo.c` | 固定间隔与指数退避加抖动的对比 |

编译（Windows，MinGW）：

```bash
gcc -std=c17 -Wall -Wextra mqtt_demo.c mqtt_packet.c -lws2_32 -o mqtt_demo
gcc -std=c17 -Wall -Wextra backoff_demo.c -o backoff_demo
```

运行：

```bash
./mqtt_demo
./backoff_demo
```

`mqtt_demo` 只绑定回环地址，端口 25731 属于本组的 25730–25739 段。

## 与固件的关系

固件里 MQTT 报文由 coreMQTT v2.3.1 生成，传输层是 ESP-AT 指令（`transport_esp_at.c`）或 PC 上的 Winsock（`transport_winsock.c`）。本目录的 `mqtt_packet.c` 不是 coreMQTT 的替代品，它的作用是把 CONNECT 等报文的每个字节摆出来讲：示例输出的十六进制与 coreMQTT 生成的报文一致。

coreMQTT、backoffAlgorithm 的源码不进仓库：

```bash
git clone --depth 1 --branch v2.3.1 https://github.com/FreeRTOS/coreMQTT.git
git clone --depth 1 --branch v1.4.1 https://github.com/FreeRTOS/backoffAlgorithm.git
```

## 连接真实的 Mosquitto

把 `mqtt_demo.c` 里的伪服务器换成 Mosquitto，或者直接用订阅者观察设备端发的报文：

```bash
mosquitto_sub -h 127.0.0.1 -t 'mdr/+/telemetry' -v
mosquitto_sub -h 127.0.0.1 -t 'mdr/+/status' -v
```