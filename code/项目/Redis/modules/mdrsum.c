/*
 * mdrsum.c —— 教程配套模块：MDR.SUMLEN key [key ...]
 *
 * 累加每个键的长度：字符串按字节数算，集合、列表、哈希、有序集合按元素个数算。
 * 键不存在时按 0 计。命令只读，不修改任何键。
 *
 * 编译（在 Redis 源码树根目录下）：
 *   gcc -fPIC -shared -O2 -Wall -Wextra -std=c11 -I src modules/mdrsum.c -o mdrsum.so
 * 加载：
 *   ./src/redis-server --loadmodule /绝对路径/mdrsum.so
 */

#include "redismodule.h"

#define MDRSUM_MODULE_NAME "mdrsum"
#define MDRSUM_MODULE_VERSION 1

/* MDR.SUMLEN key [key ...] */
static int mdr_sumlen(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 2) {
        return RedisModule_WrongArity(ctx);
    }

    long long total = 0;
    for (int i = 1; i < argc; i++) {
        RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[i], REDISMODULE_READ);
        /* 键不存在时 ValueLength 返回 0，不必单独判断。 */
        total += (long long)RedisModule_ValueLength(key);
        RedisModule_CloseKey(key);
    }
    return RedisModule_ReplyWithLongLong(ctx, total);
}

int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    REDISMODULE_NOT_USED(argv);
    REDISMODULE_NOT_USED(argc);

    if (RedisModule_Init(ctx, MDRSUM_MODULE_NAME, MDRSUM_MODULE_VERSION,
                         REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    /* firstkey=1, lastkey=-1, keystep=1：参数 1 到最后都是键。 */
    if (RedisModule_CreateCommand(ctx, "mdr.sumlen", mdr_sumlen, "readonly fast",
                                  1, -1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }
    return REDISMODULE_OK;
}
