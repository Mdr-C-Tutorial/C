# A.11 同步原语（二）：临界区、SRWLock 与条件变量：配套代码

正文里每个有输出的程序都在这里。需要 MinGW-w64 GCC（UCRT）与 64 位 Windows。

```text
mingw32-make
```

| 文件 | 正文位置 | 说明 |
| --- | --- | --- |
| `race.c` | 1 | 两个线程各加 1,000,000 次，不加同步，演示数据竞争 |
| `interlocked.c` | 2 | 用 `InterlockedIncrement` 修复 `race.c` |
| `interlocked_ret.c` | 2 | 互锁函数的返回值：新值与旧值的区别 |
| `cs_counter.c` | 3 | 用临界区对象修复 `race.c` |
| `cs_recursive.c` | 3 | 临界区对象可重复进入：进几次就要出几次 |
| `lock_cost.c` | 3 | 无争用时临界区、SRW 锁、互斥量对象各做 1,000,000 次加锁解锁的耗时 |
| `srw_try.c` | 4 | SRW 锁被持有时 `TryAcquire` 返回 0，释放后返回 1 |
| `srw_deadlock.c` | 4 | 同一线程两次获取 SRW 独占锁，程序停住 |
| `config_cache.c` | 4 | 4 个读线程共享读、1 个写线程独占写的配置表 |
| `bounded_queue.c` | 5 | 容量 4 的有界队列，2 个生产者、2 个消费者，用条件变量等待 |
| `if_vs_while.c` | 5 | 条件变量等待写成 `if` 与写成 `while` 的结果对比 |
| `once.c` | 6 | `InitOnceExecuteOnce`：4 个线程只有 1 个执行初始化 |
| `wait_on_address.c` | 6 | `WaitOnAddress` / `WakeByAddressSingle` |

`race.c` 必须在不加优化选项时编译才能稳定看到错误结果；`Makefile` 里为它单独加了 `-O0`。`wait_on_address` 需要链接 `-lsynchronization`。

所有程序在 Windows 11 x64（内部版本 26300，16 个逻辑处理器）上用 MinGW-w64 GCC 13.2 以 `-std=c17 -Wall -Wextra` 编译并运行过，输出见正文。