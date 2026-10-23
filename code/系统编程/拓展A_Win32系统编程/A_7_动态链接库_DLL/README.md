# A.7 动态链接库（DLL）：配套代码

正文里每个有输出的程序都在这里。需要 MinGW-w64 GCC（UCRT）与 64 位 Windows。

```text
mingw32-make
```

| 文件 | 正文位置 | 说明 |
| --- | --- | --- |
| `mdrmath.h`、`mdrmath.c` | 2 | 被两个程序共用的 DLL，`MDR_API` 宏切换 `dllexport` 与 `dllimport` |
| `counter_dll.c`、`counter_use.c` | 1 | DLL 的全局对象每进程一份，两次运行各从 1 数起 |
| `use_load.c` | 3 | 加载时链接：只调用函数，不出现 `LoadLibrary` |
| `use_runtime.c` | 4 | 运行时链接：`LoadLibraryW` + `GetProcAddress`，并演示错误 127 |
| `refcount.c` | 4.1 | 两次 `LoadLibraryW` 只增加引用计数，两次 `FreeLibrary` 才卸载 |
| `plugin.h`、`plugin_double.c`、`plugin_square.c`、`plugin_host.c` | 4.2 | 插件宿主：遍历 `plugins\*.dll`，按完整路径加载 |
| `trace_dll.c`、`trace_host.c` | 5 | `DllMain` 四种通知的顺序 |
| `hang_dll.c`、`hang_host.c` | 5.2 | 在 `DllMain` 里等线程，制造加载器锁死锁 |
| `search_demo.c` | 6 | `SetDefaultDllDirectories` 前后，当前目录是否参与搜索 |
| `fake_version.c`、`hijack_demo.c` | 6.2 | 程序目录里的同名 DLL 抢先被加载 |
| `noexp.c` | 2.3 | 没有任何 `dllexport` 时 MinGW 的自动导出 |
| `cast_warn.c` | 4 | 直接转换 `FARPROC` 触发 `-Wcast-function-type` |
| `arena_dll.c`、`arena_user.c` | 7 | 用私有堆分配、由 DLL 自己释放；错误释放导致堆损坏 |

`cast_warn.c` 的警告是正文要展示的现象。`hang_host.exe` 需要手动结束：`Start-Process -PassThru` 后 `WaitForExit(3000)` 返回 `False` 时调用 `$p.Kill()`。

所有程序在 Windows 11 x64（内部版本 26300）上用 MinGW-w64 GCC 13.2 以 `-std=c17 -Wall -Wextra` 编译并运行过，输出见正文。模块基址、进程 ID 每次运行都可能不同。