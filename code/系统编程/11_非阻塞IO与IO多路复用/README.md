# 11. 非阻塞 I/O 与 I/O 多路复用：配套代码

正文里每一段有输出的程序都在这里，`make` 一次编译全部。

```text
make
```

| 文件 | 正文位置 | 说明 |
| --- | --- | --- |
| `lecho_iter.c` | 1 | 阻塞式迭代 lecho，用来演示队头阻塞 |
| `holdopen.c` | 1、4 | 只建立连接、保持空闲若干秒的测试客户端 |
| `sendline.c` | 1 | 发一行、打印应答与耗时的小客户端 |
| `nonblock_read.c` | 2 | 非阻塞管道上的 `EAGAIN` |
| `busy_read.c` | 2 | 忙等统计 1 秒内的 `read` 次数 |
| `select_smoke.c` | 3 | `select` 的超时与就绪 |
| `readiness.c` | 4 | `POLLIN`/`POLLOUT`/`POLLHUP`/`POLLERR` 的几种情形 |
| `pollout_busy.c` | 4 | 一直关注 `POLLOUT` 与按需关注的 `poll` 次数对比 |
| `lecho_poll.c` | 3、4 | `poll` 版 lecho，每连接输入、输出缓冲区 |
| `lecho_epoll.c` | 5、6 | `epoll` 版 lecho，水平触发，按需关注 `EPOLLOUT` |
| `lt_et.c` | 6 | 管道上水平触发与边缘触发的差别 |
| `tcping.c` | 7 | 带超时的非阻塞 `connect` |

`exercises/` 下是习题 41105 的起始代码、客户端与压测脚本。

所有程序在 Debian 13（glibc 2.41、gcc 14.2）上以 `-std=c17 -Wall -Wextra` 编译并运行过；输出见正文。
