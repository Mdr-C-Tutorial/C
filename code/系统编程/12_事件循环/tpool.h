#ifndef TPOOL_H
#define TPOOL_H

typedef struct tpool tpool;
typedef void (*tpool_fn)(void *arg);

tpool *tpool_create(int nthreads);
void tpool_destroy(tpool *pool);
int tpool_submit(tpool *pool, tpool_fn fn, void *arg);
int tpool_shutdown(tpool *pool);

#endif