# `<signal.h>`

`<signal.h>` 提供了“信号” (Signal) 机制，用于在程序运行期间接收异步通知。信号既可能来自外部（例如用户按下 Ctrl+C 触发 `SIGINT`），也可能由程序自身触发（例如调用 `raise`）。

在标准 C 的范围内，信号主要用于：

1. **处理终止请求**：例如收到 `SIGINT` 或 `SIGTERM` 时做收尾并退出。
2. **理解异常终止路径**：例如 `abort` 往往会触发 `SIGABRT`。

::: warning 不要把信号当作异常机制

试图通过捕获 `SIGSEGV` 等信号来“恢复并继续执行”，在大多数情况下都不可靠，且很容易引入未定义行为。

:::

## 1. 核心接口

### 1.1 `signal`

```c
#include <signal.h>

void (*signal(int sig, void (*handler)(int)))(int);
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


- `sig` 是信号编号（如 `SIGINT`）。
- `handler` 有三种常见取值：
  1. `SIG_DFL`：默认处理。
  2. `SIG_IGN`：忽略该信号。
  3. 用户自定义函数：`void handler(int sig);`。

如果注册失败，`signal` 会返回 `SIG_ERR`。

### 1.2 `raise`

```c
#include <signal.h>

int raise(int sig);
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


`raise(sig)` 用于向当前进程发送一个信号 `sig`。

## 2. 标准 C 常见信号

标准 C 规定了若干宏（实现也可以提供更多）：

- `SIGABRT`：异常终止（通常来自 `abort`）。
- `SIGFPE`：算术错误（如除以 0、溢出等，具体由实现决定）。
- `SIGILL`：非法指令。
- `SIGINT`：交互式中断（通常是 Ctrl+C）。
- `SIGSEGV`：非法内存访问。
- `SIGTERM`：终止请求。

## 3. 信号处理函数应该做什么

信号处理函数运行在“异步打断”的上下文里，能安全做的事情很少。一个可移植且稳妥的策略是：

1. 定义一个 `volatile sig_atomic_t` 变量作为标志位。
2. 在处理函数里只设置该标志位。
3. 在主循环中轮询该标志位并执行真正的清理工作。

其中 `sig_atomic_t` 表示可以被原子访问的整型类型（至少在信号处理场景下是安全的）。

::: tip 为什么不用 `printf`

在信号处理函数里做 I/O、内存分配、加锁等操作，容易造成死锁或状态破坏。即便某些平台“看起来能用”，也不具备可移植性。

:::

## 4. 示例：收到 Ctrl+C 后优雅退出

```c
#include <signal.h>
#include <stdio.h>

static volatile sig_atomic_t g_stop = 0;

static void on_sigint(int sig) {
    (void)sig;
    g_stop = 1;
}

int main(void) {
    if (signal(SIGINT, on_sigint) == SIG_ERR) {
        puts("failed to register signal handler");
        return 1;
    }

    puts("press Ctrl+C to stop...");
    while (!g_stop) {
        /* busy loop for demo */
    }

    puts("stopped");
    return 0;
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


::: warning 关于 `signal` 的语义

历史上不同系统对 `signal` 的语义不完全一致（例如处理函数是否会在触发后自动恢复为默认处理）。如果你需要更可靠、可控的信号语义，通常应使用 POSIX 的 `sigaction`（见项目模块的 POSIX 章节）。

:::

## 5. 习题

1. 修改示例程序，使其在退出前打印一个统计信息（例如循环次数）；要求：统计信息的打印必须发生在主循环退出后，而不是信号处理函数中。
2. 编写程序：注册 `SIGTERM` 的处理函数；当收到 `SIGTERM` 时设置标志位并退出。
3. 编写程序：调用 `raise(SIGABRT);`，观察程序行为；再尝试先将 `SIGABRT` 设为 `SIG_IGN`，比较差异。
