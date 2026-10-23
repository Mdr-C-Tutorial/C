# A.2 字符与文本：ANSI、Unicode 与 UTF-16：配套代码

正文里每个有输出的程序都在这里。需要 MinGW-w64 GCC（UCRT）与 64 位 Windows。

```text
mingw32-make
```

| 文件 | 正文位置 | 说明 |
| --- | --- | --- |
| `ansi_vs_wide.c` | 2 | 同一段 UTF-8 字节交给 `CreateFileA` 与 `CreateFileW`，用 `FindFirstFileW` 列出两种文件名 |
| `mojibake.c` | 2 | 把 UTF-8 字节按 CP936 解码后用 `WriteConsoleW` 显示，看清乱码对应的码位 |
| `wide_bytes.c` | 3 | 同一段文字的 UTF-8 字节与 UTF-16 代码单元对照 |
| `textconv.h`、`textconv.c` | 4 | `utf8_to_wide()` 与 `wide_to_utf8()` |
| `conv.c` | 4 | 四种文本的往返转换 |
| `badconv.c` | 4 | 非法 UTF-8、孤立代理项的错误码 |
| `wargs.c` | 5 | `wmain` 取得完整 Unicode 命令行（需要 `-municode`） |
| `args_main.c` | 5 | 同一个命令行交给普通 `main` 的结果 |
| `launch_args.c` | 5 | 用 `CreateProcessW` 以固定命令行启动上面两个程序，排除 shell 的干扰 |
| `console_cp.c` | 6 | 输出代码页 936 与 65001 下同一串 UTF-8 字节的显示结果，以及 `WriteConsoleW` |
| `wprint.c` | 6 | `printf("%ls")` 在 C 区域设置下返回 -1，`setlocale` 之后正常 |
| `macrochk.c` | 1 | `UNICODE` 宏决定 `CreateFile` 指向 A 版还是 W 版 |
| `cargv.c` | 5 | `GetCommandLineW` + `CommandLineToArgvW`，不经过 CRT 的 `argv` |
| `utf8_manifest/` | 7 | `acp.c`、`spawn_utf8.c`、`utf8.manifest`、`utf8.rc`：用 windres 嵌入 UTF-8 活动代码页清单，并验证清单不传给子进程 |

构建：

```text
gcc -std=c17 -Wall -Wextra ansi_vs_wide.c -o ansi_vs_wide.exe
gcc -std=c17 -Wall -Wextra -municode wargs.c -o wargs.exe
gcc -std=c17 -Wall -Wextra conv.c textconv.c -o conv.exe
gcc -std=c17 -Wall -Wextra badconv.c textconv.c -o badconv.exe
```

`utf8_manifest/` 下的两个版本：

```text
windres utf8.rc -O coff -o utf8.res.o
gcc -std=c17 -Wall -Wextra acp.c -o acp.exe
gcc -std=c17 -Wall -Wextra acp.c utf8.res.o -o acp_utf8.exe
```

所有程序在 Windows 11 x64（内部版本 26300，16 个逻辑处理器，ANSI 代码页 936）上用 MinGW-w64 GCC 13.2 以 `-std=c17 -Wall -Wextra` 编译并运行过。控制台输出经伪控制台宿主取得，输出见正文。
