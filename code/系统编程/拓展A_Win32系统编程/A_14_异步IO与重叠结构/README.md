# A.14 异步 I/O 与重叠结构：配套代码

正文里每个有输出的程序都在这里。需要 MinGW-w64 GCC（UCRT）与 64 位 Windows。

```text
mingw32-make
```

| 文件 | 正文位置 | 说明 |
| --- | --- | --- |
| `offset_demo.c` | 2 | 重叠句柄不维护文件位置，读写起点由 `OVERLAPPED` 的 `Offset` 决定 |
| `sync_complete.c` | 3.1 | 数据已就绪时 `ReadFile` 在重叠句柄上返回 `TRUE` |
| `ov_basic.c` | 3.1 | 997（`ERROR_IO_PENDING`）与 38（`ERROR_HANDLE_EOF`） |
| `ov_chunks.c` | 3.3 | 同一文件分 4 块同时读，`WaitForMultipleObjects` 收完成 |
| `no_alert.c` | 4 | 完成例程只在可警报等待里运行：`Sleep` 与 `SleepEx` 的对比 |
| `readex_copy.c` | 4 | `ReadFileEx`/`WriteFileEx` 完成例程链式复制文件 |
| `ov_pipe_server.c` | 5 | 单线程服务两个 `\\.\pipe\mdr_echo` 实例的重叠管道服务器 |
| `pipe_client.c` | 5 | 配套的管道客户端，`-d` 指定两条消息之间的毫秒数 |
| `cancel_demo.c` | 6 | 重叠 `ConnectNamedPipe` 超时后用 `CancelIoEx` 取消，得到 995 |
| `stale_buffer.c` | 7 | 用局部缓冲区发起重叠读后返回，读完成时写进别人的栈帧 |
| `reuse_ov.c` | 7 | 同一个 `OVERLAPPED` 同时用于两个读，结果被覆盖 |
| `wait_handle.c` | 7 | 两个操作都不带事件、改等文件句柄本身，分不清谁完成了 |

`ov_pipe_server.exe` 与 `pipe_client.exe` 需要两个进程配合：先启动服务器（参数是运行秒数），再启动客户端。

所有程序在 Windows 11 x64（内部版本 26300，16 个逻辑处理器）上用 MinGW-w64 GCC 13.2 以 `-std=c17 -Wall -Wextra` 编译并运行过，输出见正文。`stale_buffer.c` 演示的是栈帧复用的行为，不同编译器与优化级别下覆盖关系不同，正文贴的是本机 `-O0` 的结果。