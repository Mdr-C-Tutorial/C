# A.15 I/O 完成端口与线程池：配套代码

正文里每个有输出的程序都在这里。需要 MinGW-w64 GCC（UCRT）与 64 位 Windows。

```text
mingw32-make
```

| 文件 | 正文位置 | 说明 |
| --- | --- | --- |
| `iocp_queue.c` | 2 | 完成端口作工作队列：8 个任务包、3 个工作线程、退出包 |
| `concurrency_probe.c` | 1.3 | 并发值 1 时，忙等任务串行、`Sleep` 任务并行 |
| `sync_success_probe.c` | 3 | `WSARecv` 立即成功时完成包仍然到达 |
| `iocp_echo.c` | 4 | 完成端口回声服务器，监听 127.0.0.1:23456 |
| `abort_probe.c` | 4.1 | `closesocket`、`CancelIoEx`、`shutdown` 对挂起读的结果 |
| `slow_client.c` | 4 | 每 300 ms 发一行、等回显再发的测试客户端 |
| `idle_client.c` | 4.1 | 连上后不发数据，用来观察取消挂起读 |
| `tp_work.c` | 5.1 | 线程池工作对象：10 个工作项复用 4 个线程 |
| `tp_timer.c` | 5.2 | 线程池计时器：每 300 ms 触发，共 5 次 |
| `tp_wait.c` | 5.3 | 线程池等待对象：不重新登记只有第一次触发 |
| `tp_io.c` | 5.4 | 线程池 I/O 对象：异步读一个文件 |

`iocp_echo` 配 A.13 的 `echo_client.exe` 测试；`tp_io` 自己生成测试文件 `tp_io_demo.txt`。

所有程序在 Windows 11 x64（内部版本 26300，16 个逻辑处理器）上用 MinGW-w64 GCC 13.2 以 `-std=c17 -Wall -Wextra` 编译并运行过，输出见正文。线程 ID 与任务分配顺序每次运行都可能不同。