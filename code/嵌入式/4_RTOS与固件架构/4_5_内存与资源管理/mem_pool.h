/*
 * mem_pool.h：固定块内存池，分配与释放都是常数时间，不产生碎片。
 *
 * 池把空闲块的序号串成一条单链表，链表的“下一个”就存在块自己的头一个字里，
 * 所以除了块存储区之外不占管理内存。lock/unlock 由调用者提供：RTOS 里传
 * taskENTER_CRITICAL/taskEXIT_CRITICAL，主机单元测试传空。
 *
 * 讲解见教程 4.5 节「内存与资源管理」。
 */
#ifndef MEM_POOL_H
#define MEM_POOL_H

#include <stddef.h>
#include <stdint.h>

#define MEM_POOL_NIL ((size_t)-1)

struct mem_pool {
    uint8_t *storage;
    size_t block_size;
    size_t block_count;
    size_t free_count;
    size_t free_head;
    void (*lock)(void);
    void (*unlock)(void);
};

/* 用调用者提供的存储区初始化内存池，block_size 会被向上对齐到指针大小 */
void mem_pool_init(struct mem_pool *pool, void *storage, size_t block_size,
                   size_t block_count, void (*lock)(void), void (*unlock)(void));

/* 取一块，池空时返回 NULL */
void *mem_pool_alloc(struct mem_pool *pool);

/* 归还一块；指针不属于本池时返回 -1（定义 MEM_POOL_DEBUG 时重复归还也返回 -1） */
int mem_pool_free(struct mem_pool *pool, void *block);

/* 剩余空闲块数 */
size_t mem_pool_free_count(struct mem_pool *pool);

#endif /* MEM_POOL_H */