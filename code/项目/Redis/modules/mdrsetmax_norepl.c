/*
 * mdrsetmax.c —— 用模块实现 SETMAX，与 §6 的核心补丁对照。
 *
 *   MDR.SETMAX_NOREPL key value
 *
 * 只在键不存在或新整数更大时写入，返回 1 或 0。写命令必须自己把
 * 效果传播给 AOF 和副本：模块命令默认不传播（call() 里对 CMD_MODULE
 * 直接跳过），要靠 RedisModule_Replicate。
 */

#include <errno.h>
#include <stdlib.h>

#include "redismodule.h"

#define MDRSETMAX_MODULE_NAME "mdrsetmax_norepl"
#define MDRSETMAX_MODULE_VERSION 1

static int parse_stored_integer(RedisModuleKey *key, long long *out) {
    size_t len;
    const char *s = RedisModule_StringDMA(key, &len, REDISMODULE_READ);
    if (s == NULL || len == 0) {
        return REDISMODULE_ERR;
    }
    char buf[32];
    if (len >= sizeof(buf)) {
        return REDISMODULE_ERR;
    }
    for (size_t i = 0; i < len; i++) {
        buf[i] = s[i];
    }
    buf[len] = '\0';
    errno = 0;
    char *end = NULL;
    long long v = strtoll(buf, &end, 10);
    if (errno != 0 || end != buf + len) {
        return REDISMODULE_ERR;
    }
    *out = v;
    return REDISMODULE_OK;
}

/* MDR.SETMAX_NOREPL key value */
static int mdr_setmax(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    long long value;
    if (RedisModule_StringToLongLong(argv[2], &value) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "ERR value is not an integer or out of range");
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (type != REDISMODULE_KEYTYPE_EMPTY && type != REDISMODULE_KEYTYPE_STRING) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(ctx, "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    if (type == REDISMODULE_KEYTYPE_STRING) {
        long long old;
        if (parse_stored_integer(key, &old) != REDISMODULE_OK) {
            RedisModule_CloseKey(key);
            return RedisModule_ReplyWithError(ctx, "ERR value is not an integer or out of range");
        }
        if (value <= old) {
            RedisModule_CloseKey(key);
            return RedisModule_ReplyWithLongLong(ctx, 0);
        }
    }

    RedisModuleString *val = RedisModule_CreateStringFromLongLong(ctx, value);
    if (RedisModule_StringSet(key, val) != REDISMODULE_OK) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(ctx, "ERR failed to set the value");
    }
    RedisModule_CloseKey(key);
    /* 故意不传播 */
    return RedisModule_ReplyWithLongLong(ctx, 1);
}

int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    REDISMODULE_NOT_USED(argv);
    REDISMODULE_NOT_USED(argc);

    if (RedisModule_Init(ctx, MDRSETMAX_MODULE_NAME, MDRSETMAX_MODULE_VERSION,
                         REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_CreateCommand(ctx, "mdr.setmax_norepl", mdr_setmax, "write deny-oom",
                                  1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }
    return REDISMODULE_OK;
}
