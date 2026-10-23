# A.18 Windows 服务、注册表与系统设施：配套代码

正文里每个程序都在这里。需要 MinGW-w64 GCC（UCRT）与 64 位 Windows。

```text
mingw32-make
```

| 文件 | 正文位置 | 说明 |
| --- | --- | --- |
| `svc_probe.c` | 2 | 只调用 `StartServiceCtrlDispatcherW`，从命令行运行时看到错误 1063 |
| `lifecycle.c` | 2 | 服务的四个状态与 `dwCheckPoint`、`dwWaitHint` 的取值 |
| `echo_svc.c` | 3 | 回声服务器 MdrEcho：服务模式 + 控制台调试模式，端口读自注册表 |
| `echo_client.c` | 3 | 最小的回声客户端，用来测试 echo_svc |
| `svc_query.c` | 4 | 用 `QueryServiceStatusEx` 查询服务状态与 PID（只读） |
| `svc_install.c` | 4 | 用 SCM API 安装、启动、停止、删除服务（需要管理员权限） |
| `reg_config.c` | 5 | HKCU 下写入、读回、枚举、删除一段配置 |
| `reg_errors.c` | 5 | 注册表的四处常见错误：长度、类型、缓冲区、缺失的项 |
| `times.c` | 6.1 | `GetSystemTimePreciseAsFileTime`、`QueryPerformanceCounter`、`GetTickCount64` |
| `sleep_grain.c` | 6.1 | `Sleep` 请求值与实际耗时的差别（系统时钟节拍） |
| `sysinfo.c` | 6.2 | `GetSystemInfo`、`GlobalMemoryStatusEx`、`RtlGetVersion` 与 `GetVersionExW` 的差别 |
| `folders.c` | 6.3 | `SHGetKnownFolderPath`、`ExpandEnvironmentStringsW`，以及把配置写到 LocalAppData |
| `eventlog.c` | 6.4 | `RegisterEventSourceW` / `ReportEventW`，只编译不运行 |

`folders.c` 需要 `-lole32 -luuid`；`echo_svc.c` 和 `echo_client.c` 需要 `-lws2_32`。`svc_install.c` 的安装、启动、停止、删除操作需要提升的管理员命令行；`svc_query.c` 只读，普通命令行即可。

除 `eventlog.c`（只编译）外，所有程序都在 Windows 11 x64（内部版本 26300，16 个逻辑处理器）上用 MinGW-w64 GCC 13.2 以 `-std=c17 -Wall -Wextra` 编译并运行过，输出见正文。`echo_svc.c` 的服务模式没有安装成服务运行，只有控制台模式实测过。