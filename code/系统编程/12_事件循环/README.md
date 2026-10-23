# 12. 事件循环：配套代码

正文里出现的程序都在这里。`make` 一次编译全部，`make test` 运行三个结果确定的程序（不启动服务器）。

```text
make
make test
```

| 文件 | 正文位置 | 说明 |
| --- | --- | --- |
| `evl.h`、`evl.c` | 2、3、4 | 事件循环库：文件事件、最小堆定时器、延迟任务 |
| `timer_demo.c` | 4.3 | 重复定时器、一次性定时器与延迟任务 |
| `eintr_epoll.c` | 5.1 | `epoll_wait` 被信号打断，设不设 `SA_RESTART` 都一样 |
| `signalfd_demo.c` | 5.3 | `signalfd` 把信号变成描述符 |
| `selfpipe_demo.c` | 5.4 | 自管道：处理函数只写一个字节 |
| `eventfd_demo.c` | 5.5 | 工作线程用 `eventfd` 唤醒循环 |
| `uaf_demo.c` | 2.3 | 同一批事件里立即 `free` 导致的释放后使用（ASan） |
| `uaf_timer.c` | 2.3 | 忘了取消定时器导致的释放后使用（ASan） |
| `uaf_fixed.c` | 2.3 | 标记关闭加延迟释放，ASan 无报告 |
| `slow_evl.c` | 6 | 回调里阻塞与交给线程池的对照，`--block` 选择阻塞版 |
| `tpool.c`、`tpool.h` | 6.2 | 线程池的最小实现：创建、提交、关闭 |
| `lecho_evl.c` | 7 | 事件循环版 lecho：空闲超时、SIGTERM 优雅退出 |
| `test_lecho.sh` | 7.6 | 用 `nc` 验证回显、空闲超时和 SIGTERM |
| `slow_reader.py` | 7.6 | 先写一批数据、延迟若干秒才读的客户端，用来观察退出时的排空 |
| `slow_client.py` | 6 | `slow_evl` 的测试客户端，两个线程分别发 `slow` 和普通行 |

编译需要 gcc 与 POSIX 环境；`epoll`、`signalfd`、`eventfd` 是 Linux 专有接口，整套代码只在 Linux 上运行。

`lecho_evl` 的用法：

```text
./lecho_evl [端口] [空闲超时毫秒]
```

默认端口 21200，默认空闲超时 10000 ms，只绑 127.0.0.1。

`slow_evl` 的用法：

```text
./slow_evl [--block] [端口]
```

默认端口 21201。`slow_client.py` 是配合它的测试客户端，需要 python3：

```text
python3 slow_client.py 21201 block
python3 slow_client.py 21201 pool
```

所有输出都在 Debian 13（glibc 2.41、gcc 14.2）上以 `-std=c17 -Wall -Wextra` 编译运行过，正文照实引用。
