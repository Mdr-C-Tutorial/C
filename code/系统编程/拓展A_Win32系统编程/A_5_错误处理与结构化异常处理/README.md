# A.5 错误处理与结构化异常处理（SEH）：配套代码

正文里每个有输出的程序都在这里。MinGW 组的程序用 MinGW-w64 GCC 13.2（UCRT）编译，MSVC 组用 Visual Studio 的命令行编译器。

```text
mingw32-make
```

| 文件 | 正文位置 | 编译器 | 说明 |
| --- | --- | --- | --- |
| `lasterror.c` | 1 | MinGW | `print_error()` 与两种失败：文件不存在（2）、目录当文件打开（5） |
| `lasterror_thread.c` | 1 | MinGW | 最后错误码每线程一份：主线程的 5 不被工作线程的 2 覆盖 |
| `lasterror_overwrite.c` | 2 | MinGW | 先记日志再取码得到 0，先取码再记日志得到 2 |
| `errcodes.c` | 3 | MinGW | 同一个失败在 Win32 码、`HRESULT`、`NTSTATUS`、`errno` 下的样子；命名对象已存在时成功也设 183 |
| `crash.c` | 4 | MinGW | `av`/`div`/`stack`/`raise` 四种未处理异常的退出码 |
| `seh_msvc.c` | 5 | MSVC | `__try`/`__except` 捕获访问违例，筛选器打印异常代码 |
| `exc_info.c` | 5 | MSVC | `GetExceptionInformation()`：访问类型、地址、出错指令 |
| `seh_nested.c` | 5、6 | MSVC | 嵌套筛选器的搜索顺序，以及 `__finally` 在展开时的执行顺序 |
| `continue_exec.c` | 5 | MSVC | `EXCEPTION_CONTINUE_EXECUTION` 不修原因时反复重试 |
| `finally_msvc.c` | 6 | MSVC | `__try`/`__finally`、`__leave`、`AbnormalTermination()` |
| `finally_paths.c` | 6 | MSVC | 正常走完、`return`、`goto` 三种退出途经 |
| `veh_dump.c` | 7 | MinGW `-ldbghelp` | VEH 打印异常，未处理筛选器写 `crash.dmp` |

MSVC 组的编译命令在开发者命令提示符里执行：

```text
cl /nologo /W4 /std:c17 seh_msvc.c
```

在 PowerShell 里先加载环境再编译，用一个两行的批处理即可：

```bat
@echo off
call "D:\Software\VS\1\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /W4 /std:c17 /utf-8 %*
```

然后 `cmd /c build_msvc.cmd seh_msvc.c`。源码里有中文注释，加 `/utf-8` 可以避免 `warning C4819`；vcvars 打印的 `vswhere.exe is not recognized` 不影响编译。

`seh_nested.c` 把 `__try`/`__finally` 嵌套在 `__try`/`__except` 的受保护块里，因为一个 `__try` 不能同时配 `__except` 与 `__finally`。

所有程序在 Windows 11 x64（内部版本 26300，16 个逻辑处理器）上编译运行过，输出见正文。`crash.c` 与 `veh_dump.c` 会崩溃，这是预期行为；`veh_dump.exe` 会在当前目录留下 `crash.dmp`，大小约 40 KB。