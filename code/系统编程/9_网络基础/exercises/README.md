# 9. 网络基础：习题配套数据

这一章没有需要运行的正文示例；这里放的是习题用到的数据文件和生成它们的程序。所有数据都由 `make_data.c` 生成，不含任何真实主机、端口或用户的信息。

```text
make data     重新生成 data/ 下的全部文件
make clean    删除生成的数据文件与程序
```

| 文件 | 用于 | 内容 |
| --- | --- | --- |
| `make_data.c` | 全部习题 | 生成下面所有数据，参数是输出目录 |
| `data/udp_frame.txt` | 40903、40904 | 一个 UDP over IPv4 over Ethernet 帧，52 字节 |
| `data/tcp_frame.txt` | 40903、40904 | 一个 TCP over IPv4 over Ethernet 帧，72 字节 |
| `data/frames_mixed.txt` | 40904 | 四个帧：好 UDP、好 TCP、负载被改的 UDP、源地址被改的 TCP |
| `data/frames_many.txt` | 40907 | 100 个帧：10 个 ARP、54 个 UDP、36 个 TCP |
| `data/segments.txt` | 40905 | 72 字节的字节流，切成 9 段，每行一段的十六进制 |
| `data/tcp_log.txt` | 40908 | 一条连接上 7 条 SEND、5 条 ACK 事件 |
| `data/tcp_stream.txt` | 40909 | 一条连接上 9 条到达记录，含乱序、重复和缺口 |
| `data/payload.txt` | 40906 | 512 行、18624 字节的文本，用来统计数据报长度 |

帧文件的格式：每行 16 个十六进制字节，帧与帧之间空一行。地址与端口是编造的，MAC 地址使用本地管理地址（`02:00:00:00:00:01` 这类形式），不会与真实设备冲突。