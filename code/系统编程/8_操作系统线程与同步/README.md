# 8. 操作系统线程与同步：配套代码

正文里出现过输出的程序都在这里，`make` 一次编译全部。需要 POSIX 环境与 pthread；
`gettid`、`pthread_setname_np`、`pthread_getaffinity_np` 是 Linux/GNU 接口，整套代码在 Linux 上运行。

```text
make
make tsan      # 单独编一份带 ThreadSanitizer 的 race_tsan
```

| 文件 | 正文位置 | 说明 |
| --- | --- | --- |
| `count_c11.c` | 1 | C11 `<threads.h>` 的计数程序，与 pthread 版对照 |
| `count_pthread.c` | 1 | pthread 版计数程序 |
| `pthread_error.c` | 1 | pthread 的错误号由返回值给出，`errno` 不动 |
| `pexit.c` | 1 | `main` 调用 `pthread_exit` 后进程不结束 |
| `arg_bug.c` | 2.1 | 把循环地址传给线程函数，四个线程读到同一个值 |
| `arg_ok.c` | 2.1 | 每个线程一个结构体，结果正确 |
| `create_join.c` | 2.2 | 用返回值从线程带出结果，`join` 后释放 |
| `detach_join.c` | 2.3 | 分离之后再 `join` 返回 `EINVAL` |
| `stacksize.c` | 2.4 | 默认栈与 256 KiB 栈的对比 |
| `tids.c` | 2.5、3.1、3.2、3.3 | `pthread_self`、`gettid`、`/proc/self/task`、线程名 |
| `shared.c` | 3.4 | 描述符共享、`errno` 与信号掩码私有 |
| `schedinfo.c` | 4.2 | 打印调度策略、nice、亲和性 |
| `affinity.c` | 4.2 | 读取 CPU 亲和性与当前 CPU |
| `ctxsw.c` | 4.3 | 睡眠线程与忙等线程的上下文切换与 CPU 时间 |
| `errorcheck.c` | 5.1 | ERRORCHECK 互斥量的 `EDEADLK` 与 `EPERM` |
| `lock_only.c` | 5.2 | 单线程 10 万次加锁，`strace` 里没有 futex |
| `futex_count.c` | 5.2 | 1～4 个线程抢同一把锁，`strace -c` 统计 futex 次数 |
| `race.c` | 5.4 | 数据竞争；`make tsan` 生成 TSan 版 |
| `cond_clock.c` | 6.2 | 条件变量时钟不一致时超时立刻返回 |
| `bqueue.c` | 6.3 | 有界阻塞队列，含单调时钟的 `bq_pop_timed` |
| `rwlock.c` | 7.1 | 读写锁保护自洽快照，`--nolock` 观察不一致 |
| `barrier.c` | 7.2 | 4 线程分三阶段，用屏障对齐 |
| `sem_once.c` | 7.3 | 无名信号量限流与 `pthread_once` |
| `robust.c` | 7.4 | 进程共享的健壮互斥量，`EOWNERDEAD` 接手 |
| `sig_handler_threads.c` | 8.2 | 进程信号由哪个线程处理 |
| `sigwait_thread.c` | 8.2 | 专门线程用 `sigwait` 接收 SIGINT/SIGTERM |
| `fork_threads.c` | 8.3 | 多线程程序 `fork` 后子进程只剩一个线程 |
| `tpool.c`、`tpool.h` | 9 | 固定大小线程池（第 12 章复用） |
| `tpool_demo.c` | 9.3 | 40 个任务、3 个工作线程、关闭后再提交 |

`exercises/buggy_pool.c` 是习题 40805 的起始代码，含数据竞争与漏通知。

几处程序的运行方式：

```text
./futex_count 1        # 单线程，没有争用
./futex_count 4        # 四线程抢同一把锁
strace -f -c -e trace=futex ./futex_count 4
./rwlock --nolock      # 读者不加锁，观察不一致的快照
./tids                 # 运行时另开一个终端执行 ps -L -o pid,lwp,comm -p PID
```

所有输出都在 Debian 13（内核 6.12、glibc 2.41、gcc 14.2）上以 `-std=c17 -Wall -Wextra` 编译运行过，正文照实引用。