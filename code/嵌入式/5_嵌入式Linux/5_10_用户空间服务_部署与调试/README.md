# 5.10 用户空间服务、部署与调试：配套代码

正文里的服务程序与配置文件都在这里。代码只在 Linux 上运行：`epoll`、`signalfd` 是 Linux 专有接口。

```text
make          # 编译 sensord、watchdog_keepalive、notify_demo
make test     # 跑一次用命名管道作数据源的完整会话
make clean    # 删除编译产物
```

| 文件 | 正文位置 | 说明 |
| --- | --- | --- |
| `sensord.c` | 1、7 | 服务程序：定期统计、SIGHUP 重载、SIGTERM 退出 |
| `evl.c`、`evl.h` | 1 | 事件循环库，取自 `code/系统编程/12_事件循环/` |
| `sensord.conf`、`sensors.txt` | 1.1 | 普通文件作数据源的配置与样例数据 |
| `inittab`、`S50sensord`、`sensord.service` | 2 | BusyBox init 与 systemd 的两种服务配置 |
| `sdnotify.c`、`sdnotify.h`、`notify_demo.c` | 2.2 | 不链接 libsystemd 的 `sd_notify` 实现与验证程序；`sensord` 用它在启动完成与每轮统计后发通知 |
| `watchdog_keepalive.c` | 3 | `/dev/watchdog` 的打开、喂狗与 magic close，只编译不运行 |
| `demo.sh` | 7 | 在宿主机上跑完整会话的脚本，输出中间产物 `demo.fifo`、`demo.conf`、`out.log`、`err.log` |

`watchdog_keepalive` 会真的打开硬件看门狗，不要在练习用的宿主机上运行；正文只要求它编译通过。`sensord` 的所有路径都相对当前目录，不会读写 `/dev`、`/sys` 或 `/proc` 下的系统文件。
