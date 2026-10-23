# A.17 控制台与 ConPTY：配套代码

正文里每个程序都在这里。需要 MinGW-w64 GCC（UCRT）与 64 位 Windows 10 1809 及以后版本。

| 文件 | 正文位置 | 说明 |
| --- | --- | --- |
| `isconsole.c` | 1 | 判断标准句柄是否连着控制台，以及 `CONOUT$` 的用法 |
| `mode_demo.c` | 2 | 保存、修改并恢复控制台输入模式 |
| `vt_color.c` | 3 | 虚拟终端序列：颜色、24 位色、原地重画一行 |
| `keys.c` | 4 | `ReadConsoleInputW` 逐条打印按键与窗口大小事件 |
| `ctrl_handler.c` | 5 | 控制处理函数跑在系统新建的线程里 |
| `stop_group.c` | 5.1 | `CREATE_NEW_PROCESS_GROUP` + `CTRL_BREAK_EVENT` 停止子进程组 |
| `conpty_host.c` | 6 | 最小的 ConPTY 宿主，打印原始字节流 |
| `conpty_run.c` | 6 | 完整的 ConPTY 宿主：输入注入、去掉控制序列，本页验证工具 |

`conpty_host.c` 与 `conpty_run.c` 要加版本宏，否则头文件不声明 `HPCON`：

```text
gcc -std=c17 -Wall -Wextra -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000006 -o conpty_host.exe conpty_host.c
gcc -std=c17 -Wall -Wextra -municode -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000006 -o conpty_run.exe conpty_run.c
```

其余程序用统一命令 `gcc -std=c17 -Wall -Wextra -o 名字.exe 名字.c`。

`conpty_run.exe` 用来运行需要真实控制台的程序（`mode_demo`、`keys`、`ctrl_handler`），用法：

```text
conpty_run [-raw] [-in 文本] [-delay 毫秒] [-timeout 毫秒] [-cols N] [-rows N] -- 命令行
```

`-in` 支持 `\r`、`\n`、`\e`、`\\`、`\xHH`，`\x03` 是 Ctrl+C。它启动时调用 `SetConsoleCtrlHandler(NULL, FALSE)` 清除继承来的「忽略 Ctrl+C」标志，并用 `STARTF_USESTDHANDLES` 加空标准句柄阻止子进程继承被重定向的句柄。

所有程序在 Windows 11 x64（内部版本 26300）上用 MinGW-w64 GCC 13.2 以 `-std=c17 -Wall -Wextra` 编译，无警告，运行输出见正文。