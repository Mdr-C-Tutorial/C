# 5.2 源码阅读：Redis SDS 与 dict

在 Redis 的系统构建中，简单动态字符串（SDS，Simple Dynamic String）与哈希字典（dict）构成了承载上层一切丰富类型的基石。从网络缓冲区的字节拼装，到键空间中成千上万个键值映射，几乎处处都能看到这两个基础设施的身影。C 语言原生的字符数组由于缺乏长度元数据且依赖空字符判定边界，在应对高性能网络协议与二进制数据时存在天然缺陷；而通用的哈希表若在扩容时粗暴搬迁，则会直接击穿单线程事件循环的延迟底线。理解 Redis 的性能特征，必须先沉入这两个数据结构的内部实现。

> 源码依据 Redis 稳定版本（提供 RSALv2 / SSPLv1 / AGPLv3 许可供选择）。本节核心讨论围绕 `sds.h`、`sds.c`、`dict.h` 与 `dict.c` 四个源码文件展开。

## 1. SDS

### 1.1 为什么需要自己的字符串

C 标准库中原生的字符串表现形式，本质上是末尾由空字符 `\0` 作为终止标记的连续字符数组。这种经典设计虽然轻便，但在 Redis 这种高并发内存服务器的高强度读写场景下，暴露出三处严重的工程痛点：

1. **无法保证二进制安全**。Redis 需要存储的内容可能涵盖图片片段、序列化对象等任意字节序列，数据流正文中完全可能出现 `\0`。诸如 `strlen`、`strcat` 等标准库函数一旦读到首个 `\0` 便认定文本终止，从而发生数据被无故截断的情况。
2. **获取长度操作具备 O(n) 开销**。标准库函数 `strlen` 必须从字符串首字符逐个扫描比对直至遇上终止符；而在高频执行的协议解析、内存审计与命令分发过程中，服务端需要以极高频度反复查询字符串长度，线性扫描的开销难以承受。
3. **拼接修改容易诱发缓冲区溢出**。标准库提供的 `strcat` 默认假定目标缓冲区已预留足够充裕的内存，内存申请与边界判定的责任全由调用者承担；在高频更新场景下，稍有疏漏便可能触发内存越界。

针对这些痛点，Redis 设计了 SDS 数据结构：在真实的字符串载荷前面安置一个结构体头部（header），显式维护当前已使用的有效长度与分配的总空间；与此同时，为了最大程度兼顾 C 语言既有生态，SDS 依旧保留了末尾写入 `\0` 终止符的习惯，这使得内部的字符指针能够无缝传入 `printf`、`strcasecmp` 等标准 C 库函数。

### 1.2 内存布局：指针指向 header 之后

::: concept sds-type-alias title="sds 的类型定义"

```c
typedef char *sds;
```

`sds` 就是 `char *`。它指向 header 之后、字符串内容的第一个字节；header 在它前面，通过负偏移 `s[-1]` 访问。

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

所有 header 都用 `__attribute__ ((__packed__))` 消除填充，使 header 紧贴字符串内容，负偏移计算精确。

:::

为了在公开的 `sds` 字符指针与隐蔽的头部结构体指针之间灵活来回转换，SDS 提供了基于宏展开的指针偏移运算：

```c
#define SDS_HDR(T,s) ((struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T))))
#define SDS_HDR_VAR(T,s) struct sdshdr##T *sh = (void*)((s)-(sizeof(struct sdshdr##T)));
```

inline 函数 `sdslen` 和 `sdsavail` 的实现十分简洁：先读取负偏移处 `s[-1] & 7` 提取低 3 位的类型标记，再根据对应类型调用宏向左跳过对应尺寸的头部，直接读取结构体中的字段：

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

::: generate sds-layout-explanation kind=explanation concepts=sds-type-alias,sds-header-family

面向有一定 C 基础的读者，用图示化的方式解释 SDS 的内存布局：一次 malloc 分配的块中，最前面是 header（len, alloc, flags），紧接着是 buf 柔性数组（就是字符串内容），末尾有一个 `\0`。sds 指针指向 buf[0]，所以可以直接当 `char *` 用，而 `s[-1]` 就是 flags 字节。解释为什么这种"指针不指向分配起始"的设计是安全的（只要记住用 `sdsfree` 释放，不要用 `free`）。解释 `__packed__` 在这里的作用——如果有填充字节，`s - sizeof(struct sdshdr8)` 算出的地址就会错。

:::

### 1.3 创建：sdsnewlen

外部接口调用 `sdsnewlen` 创建字符串时，底层核心逻辑统一汇聚在私有函数 `_sdsnewlen` 中：

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

在这段紧凑的构造逻辑中，包含三处体现工程考量的细节设计：

- **空字符串跳过 TYPE_5 直接升级为 TYPE_8**：创建空字符串通常是为了后续持续向其中追加数据。由于 TYPE_5 结构不维护 `alloc` 字段，任何追加操作都无法原地利用预留空间，每次扩容都必须强制重新分配内存，因此系统在创建之初便直接将其规避。
- **借助 `s_malloc_usable` 获取底层分配器的实际可用容量**：类似 jemalloc 这类内存分配器在实际划分内存块时，为了对齐往往会提供略大于请求字节数的物理空间。获取到实际可用容量 `bufsize` 之后，`adjustTypeIfNeeded` 会检测超额供给的空间是否已经越过当前头部类型的最大表达上限；一旦越界则相应升级为更高级别的头部，从而把分配器附赠的空闲内存全量纳入管理。
- **对外返回的指针直接定位在有效载荷 `buf` 的首地址**：由于实际分配块的基准地址起始于头部之前，对外暴露的指针向后偏移了一个结构体头部大小，保证了与标准 C 语言字符串指针的直接兼容。

### 1.4 扩容：sdsMakeRoomFor 的预分配策略

::: concept sds-prealloc title="SDS 预分配策略"

当 SDS 需要更多空间时，`sdsMakeRoomFor` 会贪心地多分配：

- 新长度 < 1MB：按新长度的两倍分配（`newlen *= 2`）
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

为什么在头部类型发生跃迁时不能简单依靠 `realloc` 完成原地扩容？根源在于不同头部结构体的物理尺寸并不一致。例如当字符串从 `sdshdr8`（头部占用 3 字节）升级到 `sdshdr16`（头部占用 5 字节）时，正文内容相对于内存基地址的偏移需要向后推移 2 个字节；而标准的 `realloc` 系统接口只能在内存块尾部追加或收缩空间，无法在头部凭空插入新的空隙。因此类型变更时，必须通过 `s_malloc_usable` 申请整块全新空间，将数据内容整体搬迁后释放旧块。

`sdsMakeRoomFor`（附带 `greedy=1`）供内部追加操作使用，采用激进的预分配策略；而 `sdsMakeRoomForNonGreedy`（`greedy=0`）则按需精确分配，供上层进行细粒度的内存控制。

### 1.5 追加：sdscatlen

`sdscatlen` 先调用 `sdsMakeRoomFor` 保证空间，再复制数据：

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

调用约定与安全警示：SDS 的所有拼接与修改接口均可能因空间不足而触发重新分配，导致底层返回的指针地址发生变动。因此在 C 语言代码中调用此类函数时，**必须始终以函数返回值覆盖原有指针**（例如显式书写 `s = sdscatlen(s, "hello", 5);`）。若遗漏对返回值的接收并继续使用旧指针，一旦底层发生内存重新安置，旧指针就会退化为野指针并诱发未定义行为。

### 1.6 收缩：sdsRemoveFreeSpace

激进的预分配策略虽然平摊了频繁追加的系统开销，但在某些长周期静置或对内存极为敏感的场景下（例如持久化写入前的只读键），多余的空闲空间会造成常驻内存浪费。此时可以通过 `sdsRemoveFreeSpace` 将 SDS 的可用容量精准压缩至正好贴合当前有效长度：

```c
sds sdsRemoveFreeSpace(sds s, int would_regrow) {
    return sdsResize(s, sdslen(s), would_regrow);
}
```

参数 `would_regrow` 用于指示该字符串在未来是否仍有再次增长的预期：若未来仍可能被追加，则收缩时会刻意避免降级为 TYPE_5，防止下次写入时再次陷入频繁重新申请内存的困境。

在调用 `sdsResize` 时，内部还专门融入了针对 jemalloc 分配器的优化：如果调用 `je_nallocx` 查询得知即将发起的 `realloc` 并不会改变分配器内部的实际划分规格，则直接略过该系统调用——在生产环境中，这能有效免除一次无意义的分配器锁竞争开销。

## 2. dict

### 2.1 核心挑战：非阻塞的渐进式迁移

在常规的哈希表实现中，一旦负载因子越过阈值，系统通常会申请一块两倍大小的连续内存，随后通过同步循环将旧表中的全部键值对一次性重新计算哈希并搬移到新表。如果哈希表中容纳了数百万乃至数千万个条目，这种全量搬迁将不可避免地导致整个进程出现数百毫秒甚至秒级的严重停顿。在 Redis 依靠单主线程驱动事件循环的体系下，任何长耗时的连续计算都会让所有并发客户端的请求陷入饥饿。

为了化解这一工程矛盾，Redis 的哈希字典 `dict` 引入了渐进式哈希重构（incremental rehashing）：在数据结构内部常态化维护两张哈希表，将庞大的数据迁移工作拆解为无数个微小的操作单元，平摊到后续每一次常规的增删查改请求以及空闲周期的定时心跳中。

### 2.2 数据结构

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

哈希表的桶数组容量始终严格约束为 2 的幂次方，在结构体中仅以指数形式存储（如指数 4 对应容量 16）。宏 `DICTHT_SIZE(exp)` 通过 `1 << exp` 快速还原真实容量，而 `DICTHT_SIZE_MASK(exp)` 则计算出 `(1 << exp) - 1` 作为按位取模的高速掩码。

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

在对外暴露的头文件中，结构体 `dictEntry` 被设计为不透明类型（Opaque Type，仅提供前向声明），其具体的字段排布收敛在 `dict.c` 内部。上层业务代码无法直接解引用其内部成员，必须经由 `dictGetKey`、`dictGetVal` 等访问器函数间接操作——这是在 C 语言中构建数据封装边界的惯用工程范式。

结构体内部的 union 联合体设计是一项典型的内存节省技巧：当键关联的值为 64 位整数或浮点数时，数据可以直接内联存储在联合体字段中，无须再单独向内存分配器申请额外的堆内存对象。

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

类型特化表 `dictType` 在功能上扮演了类似面向对象语言中“虚函数表”（vtable）的角色。根据使用场景的不同，上层调用者可以为 dict 挂配不同的函数指针集：例如用于存储业务数据的字典会以 SDS 字符串为键，并装配 SipHash 哈希算法以及基于 SDS 长度的深度对比函数；而在某些内部紧凑集合中，若调用者省略了 compare 函数，底层便直接退化为指针地址的直接比对。

:::

::: generate dict-type-explanation kind=explanation concepts=dict-type

面向有 C 基础的读者，解释 dictType 的函数指针机制如何实现类似面向对象的多态：同一套 dict 代码可以处理各种不同类型的键和值，只要调用者提供正确的 dictType。举 Redis 内部的一两个实际用法为例（比如字符串键用 SipHash + SDS compare，整数键用直接比较）。说明 `resizeAllowed` 回调的作用：dict 扩缩容前用它询问调用者是否允许这次分配，Redis 用它避免扩容使内存超过 maxmemory。fork 子进程期间限制 rehash 由 `dict_can_resize` 负责，不要和 resizeAllowed 混为一谈。

:::

### 2.3 渐进式 rehash

::: concept incremental-rehash title="渐进式 rehash 机制"

dict 始终维护两张表 `ht_table[0]` 和 `ht_table[1]`。正常状态下只使用 `ht_table[0]`，`rehashidx == -1`。

当负载因子（元素数 / 桶数）超过阈值，Redis 分配 `ht_table[1]`（大小是比当前元素数大的最小 2 的幂），将 `rehashidx` 设为 0，开始 rehash。此后每次查找、插入、删除操作都会调用 `_dictRehashStepIfNeeded`，搬迁一个桶的数据。`rehashidx` 记录下一个待搬迁的桶索引。

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

计数值 `empty_visits` 为单次迁移循环设定了访问上限（最多允许跳过 `n * 10` 个连续空桶）：当旧哈希表在经历大量删除后变得极度稀疏时，若不对空桶扫描加以限制，单步单桶的搬移操作可能会演变成对整张巨型稀疏表的深长遍历，从而背离了将开销平摊在毫秒内的设计初衷。

在执行桶内条目重分配时，`rehashEntriesInBucketAtIndex` 包含一项精妙的位运算优化：在缩容场景下（新表规模小于旧表），无需对每个节点的键重复运行高开销的哈希函数，直接将旧表中的桶下标与新表的尺寸掩码执行按位与运算，便能精确求出目标槽位：

```c
if (d->ht_size_exp[1] > d->ht_size_exp[0]) {
    h = dictGetHash(d, key) & DICTHT_SIZE_MASK(d->ht_size_exp[1]);
} else {
    h = idx & DICTHT_SIZE_MASK(d->ht_size_exp[1]);
}
```

除了在每次客户端命令交互中“顺带搬迁一步”的被动策略外，Redis 还在周期性定时任务 `serverCron` 中调用 `dictRehashMicroseconds`：在给定的微秒时间预算内（默认 1 毫秒）执行主动轮询搬迁（每小轮迁移 100 个桶），从而在业务低峰期平滑加速哈希表的整体收敛。

### 2.4 查找与插入

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
        const void *visitedKey = dictStoredKey2Key(d, dictGetKey(*link));
        if (key == visitedKey || cmpFunc(&cmpCache, key, visitedKey))
            return link;
        link = dictGetNextLink(*link);
    }
}
```

关于**插入**位置的选取：若字典正处于 rehash 状态，新节点一律直接写入新表 `ht_table[1]`；若未处于迁移状态，则写入基准表 `ht_table[0]`。新增节点始终插入对应桶单链表的表头位置——基于典型的时间局部性假设，最新写入的数据通常拥有更高的被检索概率。

完整的添加调用链为：`dictAdd` → `dictAddRaw` → `dictFindLinkForInsert`（负责定位冲突链插入点、评估扩容触发阈值，并顺带执行一次单桶搬迁）→ `dictInsertKeyAtLink`（完成节点装配与链接指针的原子回填）。

### 2.5 删除

```c
static dictEntry *dictGenericDelete(dict *d, const void *key, int nofree) {
    h = dictGetHash(d, key);
    idx = h & DICTHT_SIZE_MASK(d->ht_size_exp[0]);
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

函数参数中的 `nofree` 标志用于解耦物理删除与内存释放：标准的 `dictDelete` 会在脱链后立即销毁节点并释放键值内存；而 `dictUnlink` 仅将节点从冲突链表中摘除并递减使用计数，保留完整的节点指针交由调用者后续处理（例如在惰性释放场景中投递给后台 bio 工作线程异步销毁）。

### 2.6 扩容与缩容的触发条件

::: concept resize-policy title="扩缩容策略"

dict 的扩缩容由全局状态 `dict_can_resize` 控制，有三个档位：

- `DICT_RESIZE_ENABLE`：负载因子 >= 1 时扩容，<= 1/8 时缩容
- `DICT_RESIZE_AVOID`：负载因子 >= 4 时才强制扩容，<= 1/32 时才缩容（fork 子进程期间使用）
- `DICT_RESIZE_FORBID`：完全禁止

扩容大小：比当前元素数大的最小 2 的幂。缩容大小：不小于当前元素数的最小 2 的幂。最小桶数为 4（`DICT_HT_INITIAL_SIZE`）。

:::

::: generate resize-policy-explanation kind=explanation concepts=resize-policy,incremental-rehash

面向有一定系统编程经验的读者，解释为什么 Redis 在 fork 期间要避免 rehash：Linux 的 fork 使用 copy-on-write，父进程如果大量修改内存页（rehash 会移动大量指针），会导致页面复制，内存占用翻倍。通过切换到 DICT_RESIZE_AVOID 档位，只有在负载因子极端不合理时才允许 resize，把 COW 的额外开销降到最低。

:::

### 2.7 迭代器：safe 与 unsafe

::: concept dict-iterators title="两种迭代器"

dict 提供两种迭代器：

**Safe 迭代器**（`safe = 1`）：启动时递增 `pauserehash` 计数器，暂停 rehash。迭代期间可以安全地调用 `dictAdd`、`dictFind`、`dictDelete`。释放时递减计数器，恢复 rehash。

**Unsafe 迭代器**（`safe = 0`）：启动时记录当前 dict 的"指纹"（fingerprint），释放时重新计算指纹并 `assert` 相等。如果迭代期间有人修改了 dict，指纹不匹配，程序直接崩溃。代价更低（不需要暂停 rehash），但迭代期间只能调用 `dictNext`。

:::

指纹的计算方式是把 6 个关键值（两张表的指针、大小指数、已用数量）用 Thomas Wang 的 64 位整数 hash 链式混合：

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

任何触及字典拓扑结构的修改（包括新增节点、删除条目、推进 rehash 步进以及扩缩容重构），都会不可逆地引起这 6 个特征量中至少一项发生漂移，进而导致计算出的指纹哈希值发生突变。

### 2.8 dictScan：在 rehash 中存活的游标

::: concept dict-scan title="dictScan 的反转位递增算法"

`dictScan` 是为 Redis 的 `SCAN` 命令设计的。它的难点在于：调用者两次调用 `dictScan` 之间，dict 可能正在 rehash——表大小在变，键在搬迁。

**反转位递增**（reverse bit increment）的做法是：先把游标 `v` 的位反转，加 1，再反转回来。这个由 Pieter Noordhuis 设计的算法保证：

1. 所有桶最终都会被访问到
2. 即使表在两次调用之间扩容或缩容，也不会遗漏元素（但可能重复返回）
3. 游标 = 0 表示迭代完成

:::

`dictScan` 直接调用 `dictScanDefrag`：

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

面向有一定数据结构基础的读者，用一个具体例子解释反转位递增的工作原理。假设表大小从 4（掩码 11）扩到 8（掩码 111）：正常递增是 00 -> 01 -> 10 -> 11，反转位递增是 00 -> 10 -> 01 -> 11。当表扩到 8 时，原来桶 00 中的元素会散落到 000 和 100，桶 10 的会散落到 010 和 110——而反转位递增的遍历顺序恰好保证了：扩容后，已访问桶的扩展桶里只有已经返回过的元素，不必再访问；尚未访问的桶也不会被遗漏。解释为什么这比记录"已访问桶的集合"更高效（zero state，游标就是全部状态）。

:::

### 2.9 指针标记优化（no_value dict）

当哈希字典被特化为纯集合（配置 `no_value = 1`，即仅有键、不设值）使用时，Redis 提供了一项极其极致的内存压缩机制：若某个哈希桶中仅存在唯一的单键且没有发生哈希冲突，系统会直接将键指针嵌入桶槽指针槽位中，完全省去分配 `dictEntry` 结构体包装节点的开销。

为了在一个指针大小的槽位内无歧义地区分“当前存放的是指向 `dictEntry` 的节点指针，还是直存的键指针”，Redis 利用了现代操作系统的内存对齐约束。内部分配接口 `zmalloc` 严格保证返回的内存块按 8 字节边界对齐，这意味着任何合法分配的 `dictEntry` 结构体指针，其二进制最低 3 位必然全为 0。这闲置的低 3 位便成为了存储元数据标记的天然位图：

```c
#define ENTRY_PTR_NORMAL      0 /* 000: dictEntry 指针 */
#define ENTRY_PTR_IS_ODD_KEY  1 /* XX1: 奇数地址的键（如 SDS） */
#define ENTRY_PTR_IS_EVEN_KEY 2 /* 010: 偶数地址的键 */
```

对于 SDS 字符串而言，其首地址在物理排布上往往呈现为奇数（因为紧随在奇数尺寸的头部之后），可以直接存入而无须额外修改。偶数基地址的键则通过将最低位置 1 进行掩码编码。访问函数 `dictGetKey` 在读取时会先审查低位标记，再分发至正确的指针解码或字段取值逻辑：

```c
void *dictGetKey(const dictEntry *de) {
    if ((uintptr_t)de & ENTRY_PTR_IS_ODD_KEY) return (void *) de;
    if ((uintptr_t)de & ENTRY_PTR_IS_EVEN_KEY) return decodeMaskedPtr(de);
    return de->key;  // 正常的 dictEntry
}
```

这一针对无值字典的位标记优化在 Redis 的 Set 集合实现中被重度依赖，为海量小集合削减了相当可观的节点结构开销。

## 3. 习题

### 3.1 巩固

<Exercise id="50520" :d="2" :w="3" multi>

(1) _[2.3]_ 下表中每一行是一个刚由 `sdsnewlen` 建出、还没追加过的 SDS。把 SDS 类型、header 字节数、传给 `s_malloc_usable` 的请求字节数、`sdslen`、`sdsavail` 填出来。再写一个二十行的小程序，把 `sdsReqType` 与 `sizeof(struct sdshdrN)` 抄进去验证类型与 header 两列。

| 初始化长度 | 0 | 31 | 32 | 252 | 253 | 65530 | 65531 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 类型 | | | | | | | |
| header 字节 | | | | | | | |
| 请求字节数 | | | | | | | |
| `sdsavail` | | | | | | | |

请求字节数按 `hdrlen + initlen + 1` 算，`sdsavail` 按「分配器正好给出请求的大小」算。这张表里每一行的 `sdsavail` 都是 0，但原因不同：长度 31 那一行的 0 来自 TYPE_5 根本没有 `alloc` 字段，`sdsavail` 直接返回常量；其余几行是因为新建时没有多余空间。这正是正文说「空字符串不用 TYPE_5」以及 TYPE_5 不适合追加的原因。另外两问：长度 0 为什么不用 TYPE_5；252 与 253 之间为什么差一级 header。

(2) _[2.3]_ 按 5.1 的方式启动一个只绑本机的实例，先写入一个 64 字节的值，再追加 100 字节，两次各查一次 `DEBUG SDSLEN`：

::: terminal title="zsh - redis-cli"

```console
$ ./src/redis-cli -p 24730 SET k "$(printf 'a%.0s' $(seq 64))"
$ ./src/redis-cli -p 24730 DEBUG SDSLEN k
$ ./src/redis-cli -p 24730 APPEND k "$(printf 'b%.0s' $(seq 100))"
$ ./src/redis-cli -p 24730 DEBUG SDSLEN k
```

:::

记下两次的 `val_sds_len` 与 `val_sds_avail`，再按正文的预分配规则手算 `alloc`：`newlen = len + addlen` 之后做了哪一步，`sdsReqType(newlen)` 选到哪种 header，请求的总字节数是多少。两次的「`val_sds_len + val_sds_avail`」都比请求的总字节数减去 header 和结尾 `\0` 后多出几个字节，多出来的部分就是 `s_malloc_usable` 返回的 `bufsize` 比请求的大出的量，它被 `sdssetalloc` 一并记进了 `alloc`。本机 jemalloc 实测的一组读数是 `64/4` 与 `164/174`，你的分配器若不同，按自己的读数算。

(3) _[2.2]_ 用 `printf 'a\0b' | ./src/redis-cli -x -p 24730 SET bin` 写入一个含 `\0` 的值，记录 `STRLEN bin` 与 `DEBUG SDSLEN bin` 的结果。再写一段 C 程序，对同样的三个字节调用 `strlen`，比较两者，说明 SDS 的 `len` 字段解决了什么问题。

</Exercise>

### 3.2 编程

<Exercise id="50521" :d="4" :w="4">

写一份自己的 SDS，只实现 TYPE_5、TYPE_8、TYPE_16 三种 header，接口如下：

```c
typedef char *sds;

sds sdsnewlen(const void *init, size_t initlen);
sds sdscatlen(sds s, const void *t, size_t len);
void sdsfree(sds s);
size_t sdslen(const sds s);
size_t sdsavail(const sds s);
```

要求：

1. 一次 `malloc` 分配 header、内容与结尾的 `\0`，`sds` 指向内容首字节，类型从 `s[-1] & 7` 读出。header 加 `__attribute__((__packed__))`。
2. `sdsnewlen("", 0)` 得到 TYPE_8。扩容照正文的预分配规则：新长度小于 1MB 时翻倍，否则加 1MB。类型超过 TYPE_16 能表示的长度时返回 `NULL`。
3. 写测试程序，从空串开始连续 `sdscatlen` 一个字节一千次，打印每次真正 `realloc` 时的 `len` 与 `alloc`，最后打印总次数。按「分配器正好给请求大小」计算，总次数是 9，`alloc` 依次为 2、6、14、30、62、126、254、510、1022。再回答：真实 Redis 里 `s_malloc_usable` 拿到的块比请求的大，这会让重分配次数变多还是变少？
4. 把 `s = sdscatlen(s, ...)` 写成 `sdscatlen(s, ...)`，丢弃返回值，用 `-fsanitize=address` 编译运行。贴出报告第一行与 `#0`、`#1` 两帧，说明指针为什么失效。（ASan 的 `realloc` 每次都换地址，不必额外制造搬移。）
5. 追加 `"a\0b"` 三个字节，用 `printf("%zu %s\n", sdslen(x), x)` 打印，解释输出为什么是 `3 a`。

验收：类型表、realloc 计数输出、ASan 报告摘录、第 5 步的输出。

</Exercise>

<Exercise id="50522" :d="5" :w="4">

写一个最小 dict，把正文 2.1～2.7 的机制落到代码里：

```c
typedef struct mdict mdict;
mdict *mdict_create(void);
int mdict_add(mdict *d, const char *key);      /* 已存在返回 0 */
int mdict_contains(mdict *d, const char *key);
int mdict_delete(mdict *d, const char *key);
size_t mdict_size(const mdict *d);
int mdict_is_rehashing(const mdict *d);
```

要求：

1. 两张表 `ht[2]`，桶数始终为 2 的幂且不小于 4；元素用拉链法，新元素插到链头。
2. 键的类型通过一张回调表交给调用者，至少包含 `hash`、`compare`、`destroy` 三个函数指针，取值与正文的 `dictType` 对应。另加一个 `resizeAllowed(size_t moreMem, double ratio)`，返回 0 时本次扩容被跳过，用来模拟 Redis 在 fork 期间切换到 `DICT_RESIZE_AVOID` 的做法：构造函数多一个参数，允许把「比值达到 1 才扩容」改成「比值达到 4 才扩容」。
3. 负载因子达到 1（或按 `resizeAllowed` 调整后的阈值）时分配 `ht[1]`，大小为大于元素数的最小 2 的幂，`rehashidx` 置 0。之后每次 `add`、`contains`、`delete` 搬迁一个非空桶。连续空桶最多跳 `10 * n` 个（`n` 是本次要搬的桶数），超过就提前返回，避免稀疏表空转。
4. rehash 期间查找与删除都要查两张表，跳过 `ht[0]` 中下标小于 `rehashidx` 的桶；新增一律进 `ht[1]`。
5. 搬完 `ht[0]` 后释放旧表，`rehashidx` 回到 -1。
6. 再实现两种迭代器。safe 版在迭代期间把 `pauserehash` 加一，此时 `mdict_add` 不触发搬迁；unsafe 版在开始与结束时各算一次指纹，把两张表的指针、桶数指数、元素数共 6 个整数按正文的 Thomas Wang 混合，不一致就 `abort()`。

测试程序：

- 插入 100000 个键，键名用 `snprintf(buf, sizeof buf, "key%06d", i)` 生成；每 10000 次打印一次 `mdict_is_rehashing`、两张表的元素数与 `rehashidx`。
- 插入完成后逐个 `contains`，打印缺失个数，必须为 0；再删除一半，打印删除失败个数，必须为 0。
- safe 迭代器遍历时删除当前键，结束后元素数应当减半；unsafe 迭代器遍历时故意插入一个键，确认程序 `abort`，退出码非零。

验收：rehash 进度、缺失与删除失败计数、safe 遍历后的元素数三组输出，以及 unsafe 版 `abort` 的那次运行。

</Exercise>

### 3.3 综合与挑战

<Exercise id="50523" :d="6" :w="5" multi>

(1) _[6.5]_ 写一个 `dictScan` 模拟器，不依赖 Redis。桶数组是 16 个，每个元素记为「桶下标加序号」，实现 `rev()`、`v |= ~m0`、`rev(v)++`、`rev(v)` 这一串。

要求：

- 第一轮表大小为 4（掩码 3），扫到一半时扩到 16，旧桶里的元素按 `hash & 15` 重新分布；再扫到一半时缩回 4，按 `hash & 3` 合并。
- 游标从 0 开始，每步调用一次，返回 0 时结束。
- 记录每个元素被访问的次数。验收：每个元素至少出现一次；重复返回是允许的；打印遗漏个数（必须为 0）与重复次数最多的元素。
- 用 100 组随机的「扩容/缩容时机 + 100 个键」各跑一遍，全部无遗漏才算完成。

(2) _[4.3]_ 实现正文 2.9 的指针标记。写三个构造函数：

```c
void *make_entry(void *key);        /* 正常 dictEntry，8 字节对齐 */
void *make_odd_key(const char *s);  /* 返回奇数地址，内容以 s 开头 */
void *make_even_key(const char *s); /* 返回偶数地址 */
```

`dictGetKey` 按 `ENTRY_PTR_NORMAL`、`ENTRY_PTR_IS_ODD_KEY`、`ENTRY_PTR_IS_EVEN_KEY` 分派。验收：三个对象各取一次键，打印原始指针的低 3 位与取出的键；再把 `make_odd_key` 改成返回偶数地址，说明标记方案为什么会失效，以及 Redis 为什么能依赖「SDS 的 buf 地址是奇数」这一事实。

</Exercise>
