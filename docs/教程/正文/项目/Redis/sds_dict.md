# 源码阅读：Redis SDS 与 dict

SDS（Simple Dynamic String）和 dict 是 Redis 最基础的两个数据结构。SDS 解决 C 字符串的三个硬伤：不能存 `\0`、取长度要 O(n)、拼接容易溢出。dict 解决哈希表的一个工程难题：扩容时不能阻塞服务。两者的设计都围绕同一个主题——**用巧妙的内存布局换取运行时性能**。

> 源码来自 Redis / Valkey 最新版本（RSALv2 / SSPLv1 / AGPLv3 三选一许可）。本文基于 `sds.h`、`sds.c`、`dict.h`、`dict.c` 四个文件。

## 第一部分：SDS

### 1. 为什么需要自己的字符串

C 标准库的字符串是一个以 `\0` 结尾的 `char` 数组。这在 Redis 的使用场景下有三个致命问题：

1. **不是二进制安全的**。Redis 的值可以是任意字节序列，中间完全可能出现 `\0`。`strlen`、`strcat` 等函数遇到 `\0` 就停，会截断数据。
2. **取长度是 O(n)**。`strlen` 要从头扫到 `\0`，而 Redis 频繁需要知道字符串长度。
3. **拼接操作不安全**。`strcat` 假设目标缓冲区够大，调用者必须自己管理内存，容易溢出。

SDS 的解决方案：在字符串前面放一个 header，记录长度和已分配空间，同时保持末尾的 `\0`——这样 SDS 指针可以直接传给 `printf` 等 C 函数。

### 2. 内存布局：指针指向 header 之后

::: concept sds-type-alias title="sds 的类型定义"

```c
typedef char *sds;
```

`sds` 就是 `char *`。但它指向的不是分配块的起始地址，而是 header 之后、字符串内容的第一个字节。header 在它前面，通过负偏移 `s[-1]` 访问。

:::

::: concept sds-header-family title="sdshdr 结构体家族"

SDS 定义了五种 header，按能表示的最大长度递增：

```c
struct __attribute__ ((__packed__)) sdshdr8 {
    uint8_t len;          /* 已使用长度 */
    uint8_t alloc;        /* 总可用空间（不含 header 和末尾 \0） */
    unsigned char flags;  /* 低 3 位存类型，高 5 位保留 */
    char buf[];           /* 柔性数组：字符串内容紧跟其后 */
};
```

`sdshdr16`、`sdshdr32`、`sdshdr64` 结构相同，只是 `len` 和 `alloc` 的类型分别是 `uint16_t`、`uint32_t`、`uint64_t`。`sdshdr5` 比较特殊：它没有 `len` 和 `alloc` 字段，把长度（最多 31）编码在 `flags` 字节的高 5 位里，不能追踪剩余空间。

所有 header 都用 `__attribute__((__packed__))` 消除填充，使 header 紧贴字符串内容，负偏移计算精确。

:::

::: generate sds-layout-explanation kind=explanation concepts=sds-type-alias,sds-header-family

面向有一定 C 基础的读者，用图示化的方式解释 SDS 的内存布局：一次 malloc 分配的块中，最前面是 header（len, alloc, flags），紧接着是 buf 柔性数组（就是字符串内容），末尾有一个 `\0`。sds 指针指向 buf[0]，所以可以直接当 `char *` 用，而 `s[-1]` 就是 flags 字节。解释为什么这种"指针不指向分配起始"的设计是安全的（只要记住用 `sdsfree` 而不是 `free`）。解释 `__packed__` 在这里的作用——如果有填充字节，`s - sizeof(struct sdshdr8)` 算出的地址就会错。

:::

SDS 通过两个宏在 `sds` 指针和 header 指针之间转换：

```c
#define SDS_HDR(T,s) ((struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T))))
#define SDS_HDR_VAR(T,s) struct sdshdr##T *sh = (void*)((s)-(sizeof(struct sdshdr##T)));
```

`sdslen` 和 `sdsavail` 都是 inline 函数，先用 `s[-1] & 7` 判断类型，再用对应的宏取回 header 指针读取字段：

```c
static inline size_t sdslen(const sds s) {
    switch (sdsType(s)) {
        case SDS_TYPE_5: return SDS_TYPE_5_LEN(s);
        case SDS_TYPE_8:  return SDS_HDR(8,s)->len;
        case SDS_TYPE_16: return SDS_HDR(16,s)->len;
        case SDS_TYPE_32: return SDS_HDR(32,s)->len;
        case SDS_TYPE_64: return SDS_HDR(64,s)->len;
    }
    return 0;
}
```

### 3. 创建：sdsnewlen

`sdsnewlen` 是 SDS 的核心构造函数。它的流程：

```c
sds _sdsnewlen(const void *init, size_t initlen, int trymalloc) {
    char type = sdsReqType(initlen);
    // 空字符串不用 TYPE_5——它无法追踪剩余空间，每次追加都要重新分配
    if (type == SDS_TYPE_5 && initlen == 0) type = SDS_TYPE_8;
    int hdrlen = sdsHdrSize(type);

    void *sh = s_malloc_usable(hdrlen + initlen + 1, &bufsize);
    adjustTypeIfNeeded(&type, &hdrlen, bufsize);
    return sdsnewplacement(sh, bufsize, type, init, initlen);
}
```

几个值得注意的设计：

- **空字符串升级为 TYPE_8**。空字符串通常是为了后续追加而创建的，TYPE_5 不记录 `alloc`，每次追加都必须重新分配，所以直接跳过它。
- **`s_malloc_usable` 获取实际分配大小**。jemalloc 等分配器返回的块可能大于请求的大小。`bufsize` 拿到实际大小后，`adjustTypeIfNeeded` 会检查是否需要升级类型——比如请求 252 字节（TYPE_8），但 jemalloc 返回了 256 字节的块，就升级到 TYPE_16 以充分利用空间。
- **返回值是 `buf` 的地址**，不是分配块的起始地址。

### 4. 扩容：sdsMakeRoomFor 的预分配策略

::: concept sds-prealloc title="SDS 预分配策略"

当 SDS 需要更多空间时，`sdsMakeRoomFor` 不会只分配刚好够用的大小，而是贪心地多分配：

- 新长度 < 1MB：容量翻倍（`newlen *= 2`）
- 新长度 >= 1MB：多分配 1MB（`newlen += SDS_MAX_PREALLOC`）

这个策略使连续追加操作的均摊时间复杂度降到 O(1)。

:::

```c
sds _sdsMakeRoomFor(sds s, size_t addlen, int greedy) {
    size_t avail = sdsavail(s);
    if (avail >= addlen) return s;  // 空间够就直接返回

    size_t len = sdslen(s);
    size_t newlen = len + addlen;
    if (greedy == 1) {
        if (newlen < SDS_MAX_PREALLOC)  // 1MB
            newlen *= 2;
        else
            newlen += SDS_MAX_PREALLOC;
    }

    char type = sdsReqType(newlen);
    if (type == SDS_TYPE_5) type = SDS_TYPE_8;

    // 类型不变 → realloc；类型变了 → malloc + memcpy + free
    int use_realloc = (oldtype == type);
    if (use_realloc) {
        newsh = s_realloc_usable(sh, hdrlen + newlen + 1, &bufsize, NULL);
        s = (char*)newsh + hdrlen;
    } else {
        newsh = s_malloc_usable(hdrlen + newlen + 1, &bufsize);
        memcpy((char*)newsh + hdrlen, s, len + 1);
        s_free(sh);
        s = (char*)newsh + hdrlen;
        s[-1] = type;
        sdssetlen(s, len);
    }
    // ...
}
```

为什么类型变了不能 `realloc`？因为 header 大小变了。比如从 sdshdr8（3 字节 header）升级到 sdshdr16（5 字节 header），字符串内容需要向后移动 2 字节。`realloc` 只会在尾部扩展，不会在头部插入空间。

`sdsMakeRoomFor`（greedy=1）供内部追加操作使用；`sdsMakeRoomForNonGreedy`（greedy=0）供用户精确控制分配。

### 5. 追加：sdscatlen

理解了 `sdsMakeRoomFor` 之后，`sdscatlen` 的实现就很直白了：

```c
sds sdscatlen(sds s, const void *t, size_t len) {
    size_t curlen = sdslen(s);
    s = sdsMakeRoomFor(s, len);
    if (s == NULL) return NULL;
    memcpy(s + curlen, t, len);
    sdssetlen(s, curlen + len);
    s[curlen + len] = '\0';
    return s;
}
```

注意返回值：SDS 的所有修改操作都可能触发 `realloc`，导致指针改变。调用者**必须**用返回值替换旧指针：`s = sdscatlen(s, "hello", 5)`。如果写成 `sdscatlen(s, "hello", 5)` 而不接返回值，旧的 `s` 可能变成悬空指针。

### 6. 收缩：sdsRemoveFreeSpace

SDS 在追加时会贪心预分配，但有些场景（比如长期存储的键）需要释放多余空间。`sdsRemoveFreeSpace` 把 SDS 收缩到刚好容纳当前内容：

```c
sds sdsRemoveFreeSpace(sds s, int would_regrow) {
    return sdsResize(s, sdslen(s), would_regrow);
}
```

`would_regrow` 参数控制收缩后的类型选择：如果字符串后续还可能增长，就避免降级到 TYPE_5（因为 TYPE_5 无法记录剩余空间，下次追加必定重新分配）。

`sdsResize` 内部还针对 jemalloc 做了优化：如果 `je_nallocx` 告诉我们 `realloc` 不会改变实际分配大小，就跳过这次调用——因为即使分配大小不变，jemalloc 的 `realloc` 本身也有开销。

## 第二部分：dict

### 7. 核心问题：不阻塞的 rehash

普通哈希表在扩容时需要一次性把所有键从旧表搬到新表。如果表里有几百万个键，这个操作会阻塞服务器几百毫秒甚至更久。Redis 是单线程事件驱动模型，任何操作阻塞主线程都意味着所有客户端挂起。

dict 的解决方案：维护两张哈希表，把 rehash 分摊到每次查找/插入/删除操作中。

### 8. 数据结构

::: concept dict-struct title="dict 结构体"

```c
struct dict {
    dictType *type;              /* 类型特化：hash/比较/析构等函数指针 */
    dictEntry **ht_table[2];     /* 两张哈希表 */
    unsigned long ht_used[2];    /* 各表中的元素数量 */
    long rehashidx;              /* rehash 进度，-1 表示未在 rehash */
    unsigned pauserehash;        /* >0 则暂停 rehash（迭代器使用） */
    signed char ht_size_exp[2];  /* 表大小的指数（size = 1 << exp） */
    int16_t pauseAutoResize;     /* >0 禁止自动扩缩容 */
    void *metadata[];            /* 可选的调用者自定义数据 */
};
```

表大小始终是 2 的幂，用指数存储。`DICTHT_SIZE(exp)` 还原为 `1 << exp`，`DICTHT_SIZE_MASK(exp)` 还原为 `(1 << exp) - 1`（取模掩码）。

:::

::: concept dict-entry title="dictEntry 结构体"

```c
struct dictEntry {
    struct dictEntry *next;  /* 链表指针（拉链法） */
    void *key;
    union {
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;                     /* 值可以是指针或内联数值 */
};
```

`dictEntry` 在头文件中是 opaque 的（只有前向声明），实际定义在 `dict.c` 中。这意味着外部代码不能直接访问字段，必须通过 `dictGetKey`、`dictGetVal` 等函数——这是 C 语言中实现封装的标准手法。

值的 union 设计让 Redis 可以直接在 entry 内存储整数或浮点数，避免为简单数值额外分配内存。

:::

::: concept dict-type title="dictType：类型特化"

```c
typedef struct dictType {
    uint64_t (*hashFunction)(const void *key);
    void *(*keyDup)(dict *d, const void *key);
    void *(*valDup)(dict *d, const void *obj);
    int (*keyCompare)(dictCmpCache *cache, const void *key1, const void *key2);
    void (*keyDestructor)(dict *d, void *key);
    void (*valDestructor)(dict *d, void *obj);
    int (*resizeAllowed)(size_t moreMem, double usedRatio);
    void (*rehashingStarted)(dict *d);
    void (*rehashingCompleted)(dict *d);
    // ... 更多回调
    unsigned int no_value:1;  /* 当作集合用（无值） */
    unsigned int keys_are_odd:1;
} dictType;
```

`dictType` 是 dict 的"虚函数表"。不同用途的 dict 提供不同的函数指针组合：Redis 的主键空间用 SDS 做 key，提供 SDS 的 hash/compare/free 函数；过期字典用指针直接比较，不需要 compare。

:::

::: generate dict-type-explanation kind=explanation concepts=dict-type

面向有 C 基础的读者，解释 dictType 的函数指针机制如何实现类似面向对象的多态：同一套 dict 代码可以处理各种不同类型的键和值，只要调用者提供正确的 dictType。举 Redis 内部的一两个实际用法为例（比如字符串键用 SipHash + SDS compare，整数键用直接比较）。说明 `resizeAllowed` 回调的作用：Redis 在 fork 子进程做持久化时会禁止 rehash，以减少 copy-on-write 的内存开销。

:::

### 9. 渐进式 rehash

::: concept incremental-rehash title="渐进式 rehash 机制"

dict 始终维护两张表 `ht_table[0]` 和 `ht_table[1]`。正常状态下只使用 `ht_table[0]`，`rehashidx == -1`。

当负载因子（元素数 / 桶数）超过阈值，Redis 分配 `ht_table[1]`（大小是当前元素数向上取整到 2 的幂），将 `rehashidx` 设为 0，开始 rehash。此后每次查找、插入、删除操作都会调用 `_dictRehashStep`，搬迁一个桶的数据。`rehashidx` 记录下一个待搬迁的桶索引。

当 `ht_table[0]` 的所有元素都搬完后（`ht_used[0] == 0`），释放旧表，把 `ht_table[1]` 移到 `ht_table[0]`，`rehashidx` 重置为 -1。

:::

```c
int dictRehash(dict *d, int n) {
    int empty_visits = n * 10;

    while (n-- && d->ht_used[0] != 0) {
        assert(DICTHT_SIZE(d->ht_size_exp[0]) > (unsigned long)d->rehashidx);
        // 跳过空桶，但最多跳 10*n 个，防止空表时 CPU 空转
        while (d->ht_table[0][d->rehashidx] == NULL) {
            d->rehashidx++;
            if (--empty_visits == 0) return 1;
        }
        // 把这个桶的所有 entry 搬到 ht_table[1]
        rehashEntriesInBucketAtIndex(d, d->rehashidx);
        d->rehashidx++;
    }

    return !dictCheckRehashingCompleted(d);
}
```

`empty_visits` 限制是关键：如果旧表很稀疏（大量空桶），不加限制的话一次 rehash step 可能扫描整张表，违背"分摊"的初衷。

`rehashEntriesInBucketAtIndex` 在搬迁时有一个优化：如果是缩容（新表更小），不需要重新计算 hash，直接用旧索引与新掩码做 AND 即可：

```c
if (d->ht_size_exp[1] > d->ht_size_exp[0]) {
    h = dictGetHash(d, key) & DICTHT_SIZE_MASK(d->ht_size_exp[1]);
} else {
    h = idx & DICTHT_SIZE_MASK(d->ht_size_exp[1]);
}
```

除了"每次操作搬一步"，Redis 还在定时任务中调用 `dictRehashMicroseconds`，在给定的微秒预算内尽量多搬几轮（每轮搬 100 个桶），加速 rehash 进程。

### 10. 查找与插入

rehash 过程中，查找需要同时检查两张表：

```c
dictEntry *dictFind(dict *d, const void *key) {
    dictEntryLink link = dictFindLink(d, key, NULL);
    return (link) ? *link : NULL;
}
```

`dictFindLink` 内部的核心循环：

```c
for (table = 0; table < tables; table++) {
    // 如果 rehash 正在进行，table 0 中 rehashidx 之前的桶已经搬空
    if (table == 0 && (long)idx < d->rehashidx) continue;
    idx = hash & DICTHT_SIZE_MASK(d->ht_size_exp[table]);

    link = &(d->ht_table[table][idx]);
    while (link && *link) {
        if (key == visitedKey || cmpFunc(&cmpCache, key, visitedKey))
            return link;
        link = dictGetNextLink(*link);
    }
}
```

**插入**总是插到 `ht_table[1]`（如果正在 rehash）或 `ht_table[0]`（如果没有 rehash）。新元素插入链表头部——假设最近插入的键被访问的概率更高。

`dictAdd` 的调用链：`dictAdd` -> `dictAddRaw` -> `dictFindLinkForInsert`（查找位置 + 触发扩容检查 + 执行一步 rehash）-> `dictInsertKeyAtLink`（真正插入）。

### 11. 删除

```c
static dictEntry *dictGenericDelete(dict *d, const void *key, int nofree) {
    h = dictGetHash(d, key);
    // 顺带执行一步 rehash
    _dictRehashStepIfNeeded(d, idx);

    for (table = 0; table <= 1; table++) {
        if (table == 0 && (long)idx < d->rehashidx) continue;
        idx = h & DICTHT_SIZE_MASK(d->ht_size_exp[table]);
        he = d->ht_table[table][idx];
        prevHe = NULL;
        while (he) {
            if (key matches) {
                // 从链表中摘除
                if (prevHe)
                    dictSetNext(prevHe, dictGetNext(he));
                else
                    d->ht_table[table][idx] = dictGetNext(he);
                if (!nofree) dictFreeUnlinkedEntry(d, he);
                d->ht_used[table]--;
                _dictShrinkIfNeeded(d);  // 删除后检查是否需要缩容
                return he;
            }
            prevHe = he;
            he = dictGetNext(he);
        }
        if (!dictIsRehashing(d)) break;
    }
    return NULL;
}
```

`nofree` 参数区分两种删除模式：`dictDelete`（删除并释放）和 `dictUnlink`（只从表中摘除，不释放，让调用者先用再释放）。

### 12. 扩容与缩容的触发条件

::: concept resize-policy title="扩缩容策略"

dict 的扩缩容由全局变量 `dict_can_resize` 控制，有三个档位：

- `DICT_RESIZE_ENABLE`：负载因子 >= 1 时扩容，<= 1/8 时缩容
- `DICT_RESIZE_AVOID`：负载因子 >= 4 时才强制扩容，<= 1/32 时才缩容（fork 子进程期间使用）
- `DICT_RESIZE_FORBID`：完全禁止

扩容大小：当前元素数向上取整到 2 的幂。缩容大小：同理。最小桶数为 4（`DICT_HT_INITIAL_SIZE`）。

:::

::: generate resize-policy-explanation kind=explanation concepts=resize-policy,incremental-rehash

面向有一定系统编程经验的读者，解释为什么 Redis 在 fork 期间要避免 rehash：Linux 的 fork 使用 copy-on-write，父进程如果大量修改内存页（rehash 会移动大量指针），会导致页面复制，内存占用翻倍。通过切换到 DICT_RESIZE_AVOID 档位，只有在负载因子极端不合理时才允许 resize，把 COW 的额外开销降到最低。

:::

### 13. 迭代器：safe 与 unsafe

::: concept dict-iterators title="两种迭代器"

dict 提供两种迭代器：

**Safe 迭代器**（`safe = 1`）：启动时递增 `pauserehash` 计数器，暂停 rehash。迭代期间可以安全地调用 `dictAdd`、`dictFind`、`dictDelete`。释放时递减计数器，恢复 rehash。

**Unsafe 迭代器**（`safe = 0`）：启动时记录当前 dict 的"指纹"（fingerprint），释放时重新计算指纹并 `assert` 相等。如果迭代期间有人修改了 dict，指纹不匹配，程序直接崩溃。代价更低（不需要暂停 rehash），但迭代期间只能调用 `dictNext`。

:::

指纹的计算方式很有意思——把 6 个关键值（两张表的指针、大小指数、已用数量）用 Tomas Wang 的 64 位整数 hash 链式混合：

```c
unsigned long long dictFingerprint(dict *d) {
    unsigned long long integers[6], hash = 0;
    integers[0] = (long) d->ht_table[0];
    integers[1] = d->ht_size_exp[0];
    integers[2] = d->ht_used[0];
    integers[3] = (long) d->ht_table[1];
    integers[4] = d->ht_size_exp[1];
    integers[5] = d->ht_used[1];

    for (int j = 0; j < 6; j++) {
        hash += integers[j];
        hash = (~hash) + (hash << 21);
        hash = hash ^ (hash >> 24);
        hash = (hash + (hash << 3)) + (hash << 8);
        hash = hash ^ (hash >> 14);
        hash = (hash + (hash << 2)) + (hash << 4);
        hash = hash ^ (hash >> 28);
        hash = hash + (hash << 31);
    }
    return hash;
}
```

任何导致 dict 结构变化的操作（增删元素、rehash、扩缩容）都会改变这些值中的至少一个，从而改变指纹。

### 14. dictScan：在 rehash 中存活的游标

::: concept dict-scan title="dictScan 的反转位递增算法"

`dictScan` 是为 Redis 的 `SCAN` 命令设计的。它的难点在于：调用者两次调用 `dictScan` 之间，dict 可能正在 rehash——表大小在变，键在搬迁。

算法核心是**反转位递增**（reverse bit increment）：不是让游标 `v` 正常递增，而是先把 `v` 的位反转，加 1，再反转回来。这个由 Pieter Noordhuis 设计的算法保证：

1. 所有桶最终都会被访问到
2. 即使表在两次调用之间扩容或缩容，也不会遗漏元素（但可能重复返回）
3. 游标 = 0 表示迭代完成

:::

```c
unsigned long dictScanDefrag(dict *d, unsigned long v, ...) {
    if (!dictIsRehashing(d)) {
        m0 = DICTHT_SIZE_MASK(d->ht_size_exp[htidx0]);
        // 访问 v & m0 这个桶
        dictScanDefragBucket(d, fn, defragfns, privdata, &d->ht_table[htidx0][v & m0]);

        // 反转位递增
        v |= ~m0;       // 把掩码外的位全部置 1
        v = rev(v);      // 位反转
        v++;             // 加 1（进位会传播到高位）
        v = rev(v);      // 再反转回来
    } else {
        // rehash 中：先访问小表的桶，再访问大表中所有对应的扩展桶
        m0 = DICTHT_SIZE_MASK(d->ht_size_exp[htidx0]);  // 小表掩码
        m1 = DICTHT_SIZE_MASK(d->ht_size_exp[htidx1]);  // 大表掩码

        dictScanDefragBucket(d, ..., &d->ht_table[htidx0][v & m0]);
        do {
            dictScanDefragBucket(d, ..., &d->ht_table[htidx1][v & m1]);
            v |= ~m1;
            v = rev(v); v++; v = rev(v);
        } while (v & (m0 ^ m1));  // 遍历大表中所有高位组合
    }
    return v;
}
```

::: generate dict-scan-walkthrough kind=explanation concepts=dict-scan

面向有一定数据结构基础的读者，用一个具体例子解释反转位递增的工作原理。假设表大小从 4（掩码 11）扩到 8（掩码 111）：正常递增是 00 -> 01 -> 10 -> 11，反转位递增是 00 -> 10 -> 01 -> 11。当表扩到 8 时，原来桶 00 中的元素会散落到 000 和 100，桶 10 的会散落到 010 和 110——而反转位递增的遍历顺序恰好保证了：已经访问过的桶在扩容后的扩展桶不会被遗漏。解释为什么这比记录"已访问桶的集合"更高效（zero state，游标就是全部状态）。

:::

### 15. 指针标记优化（no_value dict）

当 dict 被用作集合（`no_value = 1`，没有值字段）时，Redis 做了一个巧妙的空间优化：如果一个桶里只有一个键，就把键指针直接存在桶槽里，不分配 `dictEntry` 结构。

区分"桶里存的是 dictEntry 指针还是直接的键指针"靠指针低位标记。`zmalloc` 保证 8 字节对齐，所以指针的低 3 位始终为 0，可以用来编码元数据：

```c
#define ENTRY_PTR_NORMAL      0 /* 000: dictEntry 指针 */
#define ENTRY_PTR_IS_ODD_KEY  1 /* XX1: 奇数地址的键（如 SDS） */
#define ENTRY_PTR_IS_EVEN_KEY 2 /* 010: 偶数地址的键 */
```

SDS 字符串的地址天然是奇数（header 大小是奇数，buf 在奇数偏移处），所以可以直接存而不需要标记。偶数地址的键需要设置第 1 位来区分。`dictGetKey` 函数根据这些标记决定如何提取键：

```c
void *dictGetKey(const dictEntry *de) {
    if ((uintptr_t)de & ENTRY_PTR_IS_ODD_KEY) return (void *) de;
    if ((uintptr_t)de & ENTRY_PTR_IS_EVEN_KEY) return decodeMaskedPtr(de);
    return de->key;  // 正常的 dictEntry
}
```

这个优化在 Redis 的集合（Set）数据类型中大量使用，能显著减少小集合的内存开销。

## 总结

SDS 和 dict 展示了两种不同维度的 C 语言设计技巧：

**SDS** 的核心思想是**隐藏 header**。通过让用户持有的指针指向字符串内容本身（而不是 header），SDS 在保持与 C 字符串兼容的同时，提供了 O(1) 取长度、二进制安全、自动扩容等能力。代价是所有修改操作都可能使指针失效，调用者必须始终使用返回值。

**dict** 的核心思想是**分摊开销**。通过维护两张表、把 rehash 拆分到每次操作中，dict 避免了传统哈希表扩容时的长时间阻塞。反转位递增的 scan 算法则解决了"无状态游标在变化中的表上迭代"这个更难的问题。

两者都大量运用了 C 语言的底层能力：柔性数组成员、packed 属性、指针算术、负偏移访问、指针低位标记、位操作——这些技巧在高级语言中无法实现，正是 C 在系统编程中不可替代的原因。
