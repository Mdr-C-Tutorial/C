# A.3 内核对象与句柄：配套代码

正文里每个有输出的程序都在这里。需要 MinGW-w64 GCC（UCRT）与 64 位 Windows。

```text
mingw32-make
```

| 文件 | 正文位置 | 说明 |
| --- | --- | --- |
| `handles.c` | 2 | 三个事件句柄的值、句柄值是否为 4 的倍数、`GetProcessHandleCount` 的变化，以及私有堆返回值不是句柄 |
| `two_handles.c` | 2 | 两个句柄指向同一个事件，关掉一个对象仍在 |
| `rights.c` | 2 | 同一对象的几个句柄访问权限不同：能 `SetEvent` 的不能等待，能等待的不能 `SetEvent` |
| `fail_values.c` | 3 | `CreateFileW`、`OpenEventW`、`CreateEventW`、`FindFirstFileW`、`CreateNamedPipeW` 失败时各自的返回值 |
| `pseudo_dup.c` | 4 | `GetCurrentProcess`/`GetCurrentThread` 的值与「关闭无效果」，`DuplicateHandle` 取得真实句柄 |
| `named_objects.c` | 5 | 同名创建返回 183、类型冲突返回 6、名字区分大小写、最后一个句柄关闭后对象消失 |
| `single_instance.c` | 5 | 命名互斥量实现单实例，可带一个「保持秒数」参数 |
| `ref_parent.c` / `ref_child.c` | 5 | 父进程关掉自己的句柄后，子进程还持有一份时命名事件仍然存在 |
| `inherit_parent.c` / `inherit_child.c` | 6 | `bInheritHandles` 与 `bInheritHandle` 两个条件都满足才继承 |
| `dup_parent.c` / `dup_child.c` | 6 | `DuplicateHandle` 把事件句柄复制进子进程 |
| `leak.c` | 7 | 一万次创建事件不关闭，句柄数一路上涨；带 `fix` 参数时每轮关闭 |
| `cleanup.c` | 7 | 用 `goto cleanup` 统一清理，正常路径与失败路径的句柄数都回到起点 |
| `private_ns.c` | 5（details） | 私有命名空间里的命名互斥量，从外面按同一名字打不开 |
| `double_close.c` | 7 | 重复关闭报错 6；句柄值被复用后会关掉别人的对象 |

`inherit_parent.exe` 与 `dup_parent.exe` 要在同目录下能找到对应的子进程程序，`Makefile` 会把它们一起编译。

所有程序在 Windows 11 x64（内部版本 26300）上用 MinGW-w64 GCC 13.2 以 `-std=c17 -Wall -Wextra` 编译并运行过，输出见正文，句柄的具体数值每次运行可能不同。