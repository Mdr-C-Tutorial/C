#ifndef TPOOL_H
#define TPOOL_H

/* 任务函数：参数由提交者提供，返回后任务即结束。 */
typedef void (*tpool_fn)(void *arg);

typedef struct tpool tpool;

/* 创建固定大小的线程池；nthreads 小于 1 时失败并把 errno 设为 EINVAL。 */
tpool *tpool_create(int nthreads);

/* 提交一个任务。队列满时提交者等待（背压），不返回错误；
 * 线程池已关闭时返回 -1，errno 为 ESHUTDOWN。 */
int tpool_submit(tpool *pool, tpool_fn fn, void *arg);

/* 不再接受新任务，等已提交的任务全部执行完后返回。 */
int tpool_shutdown(tpool *pool);

/* 关闭（如果还没有关闭）并释放线程池。 */
void tpool_destroy(tpool *pool);

#endif