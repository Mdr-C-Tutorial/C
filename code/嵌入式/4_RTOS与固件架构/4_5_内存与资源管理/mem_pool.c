/*
 * mem_pool.c：固定块内存池的实现。
 *
 * 空闲块用单链表串起来，链表的“下一个”存在块自己的头一个字里；分配就是
 * 取下表头，释放就是插回表头，两者都不循环、不合并，耗时与池的大小无关。
 *
 * 讲解见教程 4.5 节「内存与资源管理」。
 */
#include "mem_pool.h"

#include <string.h>

#define ALIGN_UP(x, a) (((x) + (a) - 1u) & ~((a) - 1u))

static uint8_t *block_at(const struct mem_pool *pool, size_t index) {
    return pool->storage + index * pool->block_size;
}

static size_t index_of(const struct mem_pool *pool, const void *block) {
    const uint8_t *p = block;
    const uint8_t *base = pool->storage;
    size_t total = pool->block_size * pool->block_count;

    if (p < base || p >= base + total) {
        return MEM_POOL_NIL;
    }
    if ((size_t)(p - base) % pool->block_size != 0u) {
        return MEM_POOL_NIL;
    }
    return (size_t)(p - base) / pool->block_size;
}

void mem_pool_init(struct mem_pool *pool, void *storage, size_t block_size,
                   size_t block_count, void (*lock)(void), void (*unlock)(void)) {
    size_t aligned = ALIGN_UP(block_size, sizeof(void *));

    pool->storage = storage;
    pool->block_size = aligned;
    pool->block_count = block_count;
    pool->lock = lock;
    pool->unlock = unlock;

    /* 从后往前串：第 0 块在表头，分配时先给出地址最低的块，便于观察 */
    for (size_t i = block_count; i > 0u; --i) {
        size_t *next = (size_t *)block_at(pool, i - 1u);
        *next = (i == block_count) ? MEM_POOL_NIL : i;
    }
    pool->free_head = 0u;
    pool->free_count = block_count;
}

void *mem_pool_alloc(struct mem_pool *pool) {
    void *result = NULL;

    if (pool->lock != NULL) {
        pool->lock();
    }

    if (pool->free_head != MEM_POOL_NIL) {
        size_t index = pool->free_head;
        size_t *next = (size_t *)block_at(pool, index);
        pool->free_head = *next;
        pool->free_count--;
        *next = MEM_POOL_NIL;
        result = next;
    }

    if (pool->unlock != NULL) {
        pool->unlock();
    }
    return result;
}

int mem_pool_free(struct mem_pool *pool, void *block) {
    size_t index = index_of(pool, block);

    if (index == MEM_POOL_NIL) {
        return -1;
    }

    if (pool->lock != NULL) {
        pool->lock();
    }

#ifdef MEM_POOL_DEBUG
    /* 调试版：沿空闲链表找一遍，重复归还时直接拒绝 */
    for (size_t i = pool->free_head; i != MEM_POOL_NIL;
         i = *(size_t *)block_at(pool, i)) {
        if (i == index) {
            if (pool->unlock != NULL) {
                pool->unlock();
            }
            return -1;
        }
    }
#endif

    size_t *next = (size_t *)block_at(pool, index);
    *next = pool->free_head;
    pool->free_head = index;
    pool->free_count++;

    if (pool->unlock != NULL) {
        pool->unlock();
    }
    return 0;
}

size_t mem_pool_free_count(struct mem_pool *pool) {
    return pool->free_count;
}