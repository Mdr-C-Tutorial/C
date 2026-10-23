/*
 * test_mem_pool.c：mem_pool 的主机单元测试。
 *
 * 编译运行（不链接 FreeRTOS）：
 *   gcc -std=c17 -Wall -Wextra -DMEM_POOL_DEBUG mem_pool.c test_mem_pool.c -o test_mem_pool
 *
 * 讲解见教程 4.5 节「内存与资源管理」。
 */
#include <stdio.h>
#include <string.h>

#include "mem_pool.h"

#define BLOCK_SIZE 32u
#define BLOCK_COUNT 8u

static uint8_t g_storage[BLOCK_SIZE * BLOCK_COUNT];

static int g_failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);           \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)

static void test_alloc_exhaust(void) {
    struct mem_pool pool;
    void *blocks[BLOCK_COUNT];

    mem_pool_init(&pool, g_storage, BLOCK_SIZE, BLOCK_COUNT, NULL, NULL);
    CHECK(mem_pool_free_count(&pool) == BLOCK_COUNT);

    for (size_t i = 0; i < BLOCK_COUNT; ++i) {
        blocks[i] = mem_pool_alloc(&pool);
        CHECK(blocks[i] != NULL);
        CHECK(mem_pool_free_count(&pool) == BLOCK_COUNT - i - 1u);
    }
    /* 耗尽后再取，必须返回 NULL 而不是越界 */
    CHECK(mem_pool_alloc(&pool) == NULL);

    /* 每块地址都落在存储区内，且互不重叠 */
    for (size_t i = 0; i < BLOCK_COUNT; ++i) {
        CHECK(blocks[i] >= (void *)g_storage);
        CHECK(blocks[i] < (void *)(g_storage + sizeof g_storage));
        for (size_t j = i + 1u; j < BLOCK_COUNT; ++j) {
            CHECK(blocks[i] != blocks[j]);
        }
    }
    printf("test_alloc_exhaust: 通过\n");
}

static void test_free_then_alloc(void) {
    struct mem_pool pool;
    void *a;
    void *b;

    mem_pool_init(&pool, g_storage, BLOCK_SIZE, BLOCK_COUNT, NULL, NULL);
    a = mem_pool_alloc(&pool);
    b = mem_pool_alloc(&pool);

    CHECK(mem_pool_free(&pool, a) == 0);
    CHECK(mem_pool_free_count(&pool) == BLOCK_COUNT - 1u);
    /* 刚归还的块能再取出来，池容量没有减少 */
    CHECK(mem_pool_alloc(&pool) == a);
    CHECK(mem_pool_free(&pool, a) == 0);
    CHECK(mem_pool_free(&pool, b) == 0);
    CHECK(mem_pool_free_count(&pool) == BLOCK_COUNT);
    printf("test_free_then_alloc: 通过\n");
}

static void test_bad_free(void) {
    struct mem_pool pool;
    int local;
    void *a;

    mem_pool_init(&pool, g_storage, BLOCK_SIZE, BLOCK_COUNT, NULL, NULL);
    a = mem_pool_alloc(&pool);

    /* 池外的指针、块中间的地址、重复归还，都应该被拒绝 */
    CHECK(mem_pool_free(&pool, &local) == -1);
    CHECK(mem_pool_free(&pool, g_storage + 1) == -1);
    CHECK(mem_pool_free(&pool, a) == 0);
    CHECK(mem_pool_free(&pool, a) == -1);
    CHECK(mem_pool_free_count(&pool) == BLOCK_COUNT);
    printf("test_bad_free: 通过\n");
}

static void test_write_through(void) {
    struct mem_pool pool;
    char *a;
    char *b;

    mem_pool_init(&pool, g_storage, BLOCK_SIZE, BLOCK_COUNT, NULL, NULL);
    a = mem_pool_alloc(&pool);
    b = mem_pool_alloc(&pool);
    CHECK(a != NULL && b != NULL);
    if (a == NULL || b == NULL) {
        return;
    }

    memset(a, 'A', BLOCK_SIZE);
    memset(b, 'B', BLOCK_SIZE);
    /* 两块不重叠：写满一块不会碰到另一块 */
    CHECK(a[0] == 'A' && a[BLOCK_SIZE - 1u] == 'A');
    CHECK(b[0] == 'B' && b[BLOCK_SIZE - 1u] == 'B');
    printf("test_write_through: 通过\n");
}

int main(void) {
    test_alloc_exhaust();
    test_free_then_alloc();
    test_bad_free();
    test_write_through();

    if (g_failures == 0) {
        printf("全部测试通过\n");
        return 0;
    }
    printf("%d 项检查失败\n", g_failures);
    return 1;
}