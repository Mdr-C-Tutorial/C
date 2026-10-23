# 第 16 章的事件与注入缺陷

本章的快照增加了 `include/wf/export.h`（WF_API 导出标记）、`src/config.h.in`、
`CMakePresets.json`，`CMakeLists.txt` 按编译器选择警告选项并检测 `getline`。
`app/main.c` 有两条读文件路径：检测到 `getline` 时按行读，否则用 `fgets` 分块读。

## (a) 把 dup_string 换成 strdup

`counter_strdup.c` 是 `src/counter.c` 的变体：`dup_string` 被换成 `strdup`。

Windows（MinGW-w64 GCC 13.2，CMake 默认生成 `-std=gnu17`）照常编译、测试通过：

```text
test_token: 27 checks, 0 failed
test_counter: 36 checks, 0 failed
sample: ok
```

Linux（GCC 14.2，`-std=c17` 且 `C_EXTENSIONS OFF`）构建失败：

```text
/home/minsecrus/tutorial/work/snap-chain/ch16/wordfreq/src/counter.c:87:18: error: implicit declaration of function ‘strdup’; did you mean ‘strcmp’? [-Wimplicit-function-declaration]
/home/minsecrus/tutorial/work/snap-chain/ch16/wordfreq/src/counter.c:87:18: error: initialization of ‘char *’ from ‘int’ makes pointer from integer without a cast [-Wint-conversion]
```

strdup 属于 POSIX，C23 起才进入标准库；glibc 在 `-std=c17` 严格模式下不声明它，
MinGW 的 UCRT 头文件照常声明。GCC 14 起调用未声明的函数是错误。

修复有三条路：在文件首行定义 `_POSIX_C_SOURCE 200809L`、保留自写的三行函数、
或者配置时检测。本章采用第三种（见下面的 getline）。

## (b) include 的大小写写错

`include_case_mismatch.c` 里写的是 `#include "wf/Counter.h"`，实际文件名是 `counter.h`。

Windows 上能编译（文件系统不区分大小写，退出状态 0）；Linux 上：

```text
icm.c:9:10: fatal error: wf/Counter.h: No such file or directory
    9 | #include "wf/Counter.h"
      |          ^~~~~~~~~~~~~~
compilation terminated.
```

## getline 的两条路径

```sh
cmake -S . -B build                 # 检测到 getline（Linux）
cmake -S . -B build-nogetline -DWF_HAVE_GETLINE=OFF   # 强制回退到 fgets
```

两条路径在 SignalX（GCC 14.2）上都通过 ctest，样例输出相同：

```text
      4 the
      2 on
      2 sat
```
