# 源码阅读：Redis 跳表

Redis 的有序集合（Sorted Set / ZSET）是最常用的 Redis 数据结构之一，支持按分数排序、按排名查询、范围检索等操作，全部在 O(log N) 时间内完成。它的核心就是一个**跳表**（skiplist）。

本文从**设计动机到实现细节**自顶向下地拆解 Redis 跳表的源码。读完后你会理解：跳表解决了什么问题，Redis 选择它的理由，以及每一个关键操作的 C 实现。

> 源码基于 Redis 8.x（`t_zset.c` + `server.h`）。跳表实现集中在 `t_zset.c` 前 ~700 行。

## 1. 为什么用跳表

::: generate skiplist-intro kind=explanation concepts=skiplist-node-struct,skiplist-struct

面向有链表和二叉搜索树基础的 C 学习者，解释跳表的核心思想：在有序链表上建立多层"快速通道"索引，每一层都是下一层的子集，从顶层开始查找可以跳过大量节点，将 O(N) 的链表查找降到 O(log N)。用电梯楼层或地铁快线的类比帮助理解。

:::

Redis 的作者 antirez 在多个场合解释过为什么选择跳表而不是红黑树：

> They are simpler, and about as fast.

具体来说，跳表有三个实际优势：

1. **实现简单**：红黑树的旋转、着色规则比跳表的随机层数复杂得多，代码量少意味着 bug 少。
2. **天然支持范围查询**：跳表的底层是有序链表，`ZRANGEBYSCORE` 只需定位起点后沿链表向前遍历。红黑树做范围查询需要中序遍历，没这么直接。
3. **易于并发修改**：跳表的局部修改只涉及相邻节点的指针，加锁粒度天然更细。

::: generate skiplist-vs-bst kind=comparison concepts=skiplist-node-struct,skiplist-struct

面向有数据结构基础的读者，对比跳表和平衡二叉搜索树（红黑树/AVL树）在以下维度的差异：时间复杂度（查找/插入/删除均为 O(log N)）、实现复杂度、范围查询效率、内存开销、并发友好度。结论是二者性能相当，但跳表在工程上更简单。

:::

## 2. 数据结构

### 2.1 跳表节点

::: concept skiplist-node-struct title="zskiplistNode 结构"

```c
typedef struct zskiplistNode {
    double score;                       // 分数，排序的依据
    struct zskiplistNode *backward;     // 后退指针，仅在第 0 层
    struct zskiplistLevel {
        struct zskiplistNode *forward;  // 前进指针
        unsigned long span;             // 跨度：到 forward 节点之间跳过的节点数
    } level[];                          // 柔性数组，长度 = 该节点的层数
    // sds ele 嵌入在 level[] 之后（单次分配）
} zskiplistNode;
```

每个节点存储：
- **score**：`double` 类型的分数，是排序的第一关键字。
- **ele**：SDS 字符串，是排序的第二关键字（分数相同时按字典序）。在最新版 Redis 中，ele 不再是独立指针，而是直接嵌入在节点分配的末尾，减少一次 `malloc`。
- **backward**：后退指针，仅存在于第 0 层，支持从尾到头的反向遍历（`ZREVRANGE` 需要）。
- **level[]**：柔性数组成员。每一层包含一个 `forward` 前进指针和一个 `span` 跨度值。

:::

`span`（跨度）是 Redis 跳表相对于教科书版本的关键扩展。它记录的是**两个节点之间跳过了多少个节点**。累加路径上的 span 就能得到节点的排名（rank），这让 `ZRANK` 命令实现为 O(log N)。

节点创建时的单次分配策略值得一看：

```c
static zskiplistNode *zslCreateNode(zskiplist *zsl, int level,
                                     double score, sds ele) {
    size_t node_size = sizeof(zskiplistNode)
                     + level * sizeof(struct zskiplistLevel);
    size_t sds_buf_size = sds_hdr_len + ele_len + 1;
    size_t total_size = node_size + sds_buf_size;

    // 一次 malloc 分配 node + level[] + 嵌入的 sds
    zskiplistNode *zn = zmalloc_usable(total_size, &usable);

    zn->score = score;
    zn->backward = NULL;

    // 在 node 末尾就地构造 sds
    char *sds_buf = (char*)zn + node_size;
    sds embedded_sds = sdsnewplacement(sds_buf, sds_buf_size,
                                        sds_type, ele, ele_len);
    // 层数和 sds 偏移存入 level[0].span（第 0 层的 span 字段复用为元信息）
    zslSetNodeInfo(zn, level, sds_offset);
    return zn;
}
```

一个节点只做一次内存分配，`struct` 本体、`level[]` 数组、字符串数据全部连续存放。释放时也只需一次 `zfree`。

### 2.2 跳表本体

::: concept skiplist-struct title="zskiplist 结构"

```c
typedef struct zskiplist {
    struct zskiplistNode *header, *tail;  // 头节点（哨兵）和尾节点
    unsigned long length;                  // 节点总数（不含 header）
    int level;                             // 当前最高层数
    size_t alloc_size;                     // 跟踪内存占用
} zskiplist;
```

- **header** 是一个拥有 `ZSKIPLIST_MAXLEVEL`（32）层的哨兵节点，不存储数据，作为所有层的起点。
- **tail** 指向最后一个节点，支持 O(1) 访问末尾。
- **level** 记录当前实际使用的最高层数，查找从这一层开始而不是从 32 层开始。

:::

### 2.3 有序集合的双重索引

有序集合由跳表和哈希表组成：

```c
typedef struct zset {
    dict *dict;       // ele -> score 的哈希表，O(1) 按成员查分数
    zskiplist *zsl;   // score -> ele 的跳表，支持排序和范围查询
} zset;
```

同一个元素同时存在于两个结构中，且共享同一份 SDS 字符串。`ZSCORE` 走哈希表，`ZRANGE` 走跳表，各取所长。

## 3. 随机层数

::: concept random-level title="zslRandomLevel 的几何分布"

```c
#define ZSKIPLIST_MAXLEVEL 32
#define ZSKIPLIST_P 0.25

static int zslRandomLevel(void) {
    static const int threshold = ZSKIPLIST_P * RAND_MAX;
    int level = 1;
    while (random() < threshold)
        level += 1;
    return (level < ZSKIPLIST_MAXLEVEL) ? level : ZSKIPLIST_MAXLEVEL;
}
```

每个节点从 level 1 开始，每次有 25% 的概率升一层，直到达到上限 32。这是一个参数 p=0.25 的几何分布：

- 100% 的节点在第 1 层
- 25% 的节点在第 2 层
- 6.25% 的节点在第 3 层
- ...
- 第 k 层的期望节点数 = N * 0.25^(k-1)

上限 32 层足够支撑 2^64 个元素（4^32 = 2^64）。

:::

::: generate random-level-vis kind=explanation concepts=random-level

面向 C 学习者，用一个 8 节点的例子直观展示 zslRandomLevel 产生的跳表结构：大部分节点只有 1 层，少数有 2 层，极少数有 3 层或更多。解释为什么 p=0.25（而不是 0.5）是一个好的权衡：更少的指针意味着更低的内存开销，同时查找效率只略有降低。计算平均每个节点的指针数为 1/(1-p) = 1.33 个。

:::

## 4. 插入

::: concept insert-invariant title="插入算法的核心不变量"

插入的核心是维护两个数组：
- `update[i]`：第 i 层中，新节点应插入在其**之后**的那个节点。
- `rank[i]`：到达 `update[i]` 时经过的节点数（用于计算 span）。

插入分三步：(1) 从最高层到第 0 层逐层查找插入位置，填充 update[] 和 rank[]；(2) 生成随机层数，创建新节点；(3) 在每一层执行链表插入并更新 span。

:::

`zslInsert` 是入口，`zslInsertNode` 包含核心逻辑：

```c
zskiplistNode *zslInsert(zskiplist *zsl, double score, sds ele) {
    int level = zslRandomLevel();
    zskiplistNode *node = zslCreateNode(zsl, level, score, ele);
    zslInsertNode(zsl, node);
    return node;
}
```

### 4.1 查找插入位置

```c
static void zslInsertNode(zskiplist *zsl, zskiplistNode *node) {
    zskiplistNode *update[ZSKIPLIST_MAXLEVEL];
    unsigned long rank[ZSKIPLIST_MAXLEVEL];
    zskiplistNode *x;
    int i, level;
    double score = node->score;
    sds ele = zslGetNodeElement(node);
    level = zslGetNodeInfo(node)->levels;

    // 从最高层开始，逐层下降，找到每层的前驱节点
    x = zsl->header;
    for (i = zsl->level-1; i >= 0; i--) {
        rank[i] = i == (zsl->level-1) ? 0 : rank[i+1];
        while (zslCompareWithNode(score, ele, x->level[i].forward) > 0) {
            rank[i] += zslGetNodeSpanAtLevel(x, i);
            x = x->level[i].forward;
        }
        update[i] = x;
    }
```

查找路径是一条从左上到右下的阶梯：在每一层尽量向右走（前进指针指向的节点比目标小就继续），走不动了就下降一层。`rank[i]` 累加了到达 `update[i]` 经过的总节点数。

### 4.2 链接新节点

```c
    // 如果新节点层数超过当前最高层，补充 update 和 rank
    if (level > zsl->level) {
        for (i = zsl->level; i < level; i++) {
            rank[i] = 0;
            update[i] = zsl->header;
            zslSetNodeSpanAtLevel(update[i], i, zsl->length);
        }
        zsl->level = level;
    }

    // 逐层插入
    for (i = 0; i < level; i++) {
        node->level[i].forward = update[i]->level[i].forward;
        update[i]->level[i].forward = node;

        // 新节点的 span = 原 span - 已跨越的距离
        zslSetNodeSpanAtLevel(node, i,
            zslGetNodeSpanAtLevel(update[i], i) - (rank[0] - rank[i]));
        // 前驱的 span = 已跨越的距离 + 1
        zslSetNodeSpanAtLevel(update[i], i, (rank[0] - rank[i]) + 1);
    }

    // 未被新节点覆盖的高层，span 只需 +1
    for (i = level; i < zsl->level; i++) {
        zslIncrNodeSpanAtLevel(update[i], i, 1);
    }
```

span 的更新公式是理解插入的关键。设新节点插入位置的排名为 `rank[0]`，而 `update[i]` 的排名为 `rank[i]`：

- `update[i]` 原来的 span 覆盖了从 `update[i]` 到下一个节点的距离。
- 新节点插在中间，把这段距离分成两段：`rank[0] - rank[i]` 是 `update[i]` 到新节点的距离，剩余部分归新节点。
- `+1` 是因为新节点本身也占一个位置。

::: generate insert-visualization kind=example concepts=insert-invariant

面向 C 学习者，用一个具体的 5 节点跳表（score 分别为 1, 3, 5, 7, 9），逐步演示插入 score=4 的过程：展示 update[] 数组在每一层记录的前驱节点，rank[] 的累加过程，随机层数的生成，以及 span 的更新计算。用文字伪图（ASCII art）画出插入前后的跳表结构对比。

:::

### 4.3 更新后退指针

```c
    node->backward = (update[0] == zsl->header) ? NULL : update[0];
    if (node->level[0].forward)
        node->level[0].forward->backward = node;
    else
        zsl->tail = node;

    zsl->length++;
}
```

后退指针只在第 0 层维护。如果新节点成为最后一个节点，更新 `zsl->tail`。

## 5. 删除

::: concept delete-invariant title="删除算法的不变量"

删除与插入对称：先查找 update[] 数组（每层的前驱），然后逐层解除链接。span 的更新也是镜像操作——前驱的 span 要加上被删节点的 span 再减 1（因为少了一个节点）。

:::

删除分为两步：`zslDelete` 找到前驱，`zslUnlinkNode` 解除链接：

```c
static void zslUnlinkNode(zskiplist *zsl, zskiplistNode *x,
                           zskiplistNode **update) {
    for (int i = 0; i < zsl->level; i++) {
        if (update[i]->level[i].forward == x) {
            // 这一层直接链接到 x，合并 span 并跳过 x
            zslIncrNodeSpanAtLevel(update[i], i,
                zslGetNodeSpanAtLevel(x, i) - 1);
            update[i]->level[i].forward = x->level[i].forward;
        } else {
            // 这一层没有链接到 x（x 的层数不够高），只需 span-1
            zslDecrNodeSpanAtLevel(update[i], i, 1);
        }
    }

    // 更新后退指针
    if (x->level[0].forward)
        x->level[0].forward->backward = x->backward;
    else
        zsl->tail = x->backward;

    // 如果删除的是最高层节点，降低跳表层数
    while (zsl->level > 1 &&
           zsl->header->level[zsl->level-1].forward == NULL)
        zsl->level--;

    zsl->length--;
}
```

`if/else` 分支处理了一个细节：如果节点 x 只有 2 层，但跳表有 5 层，那么第 3~5 层的 `update[i]` 并不直接指向 x。这些层只需将 span 减 1，不需要改变 forward 指针。

Redis 还提供了一个巧妙的分数更新优化 `zslUpdateScore`：如果新分数不改变节点的相对位置（比前驱大、比后继小），直接原地修改 score，避免删除再插入的开销。

## 6. 排名与范围查询

### 6.1 排名查询

`zslGetRank` 利用 span 累加实现 O(log N) 的排名计算：

```c
unsigned long zslGetRank(zskiplist *zsl, double score, sds ele) {
    zskiplistNode *x;
    unsigned long rank = 0;

    x = zsl->header;
    for (int i = zsl->level-1; i >= 0; i--) {
        while (zslCompareWithNode(score, ele, x->level[i].forward) >= 0) {
            rank += zslGetNodeSpanAtLevel(x, i);
            x = x->level[i].forward;
        }
        if (x != zsl->header &&
            zslCompareWithNode(score, ele, x) == 0)
            return rank;
    }
    return 0;  // 未找到
}
```

查找路径和插入相同——从最高层到最低层的阶梯式遍历。每次沿前进指针前进时，累加当前层的 span。到达目标节点时，累加值就是其 1-based 排名。

反向操作 `zslGetElementByRank` 同样利用 span 在 O(log N) 内按排名定位节点。

### 6.2 范围查询

`zslNthInRange` 查找指定分数范围内的第 N 个元素。它的策略是：

1. 先用 `zslIsInRange` 快速判断范围是否和跳表有交集（只看头尾）。
2. 从最高层开始，找到范围的边界节点。
3. 如果偏移量 N 较小（< `ZSKIPLIST_MAX_SEARCH`=10），直接沿第 0 层逐个走。
4. 如果 N 较大，利用高层指针跳跃定位。

```c
zskiplistNode *zslNthInRange(zskiplist *zsl, zrangespec *range,
                              long n, unsigned long *out_rank) {
    if (!zslIsInRange(zsl, range)) return NULL;

    // 在最高层找到范围边界
    x = zsl->header;
    i = zsl->level - 1;
    while (x->level[i].forward &&
           !zslValueGteMin(x->level[i].forward->score, range)) {
        edge_rank += zslGetNodeSpanAtLevel(x, i);
        x = x->level[i].forward;
    }
    // 记住此节点以备大偏移量时使用
    last_highest_level_node = x;

    // 继续在低层精确定位
    for (i = zsl->level - 2; i >= 0; i--) {
        while (x->level[i].forward &&
               !zslValueGteMin(x->level[i].forward->score, range)) {
            edge_rank += zslGetNodeSpanAtLevel(x, i);
            x = x->level[i].forward;
        }
    }

    // 小偏移：逐节点走
    if (n < ZSKIPLIST_MAX_SEARCH) {
        for (i = 0; i < n + 1; i++)
            x = x->level[0].forward;
    } else {
        // 大偏移：从高层节点跳跃
        x = zslGetElementByRankFromNode(last_highest_level_node,
                                         zsl->level - 1, rank_diff);
    }
    // ...
}
```

当 `n` 为负数时，函数从范围末尾用 backward 指针反向遍历，支持 `ZREVRANGEBYSCORE` 的语义。

## 7. 从命令到跳表操作

有序集合的常用命令与跳表操作的对应关系：

| Redis 命令 | 操作 | 跳表函数 |
|:---|:---|:---|
| `ZADD key score member` | 插入/更新分数 | `zslInsert` 或 `zslUpdateScore` |
| `ZREM key member` | 删除 | `zslDelete` |
| `ZSCORE key member` | 查分数 | 不走跳表，走 `dict` 哈希表 |
| `ZRANK key member` | 查排名 | `zslGetRank` |
| `ZRANGE key start stop` | 按排名范围 | `zslGetElementByRank` + 遍历 |
| `ZRANGEBYSCORE key min max` | 按分数范围 | `zslNthInRange` + 遍历 |
| `ZRANGEBYLEX key min max` | 按字典序范围 | `zslNthInLexRange` + 遍历 |
| `ZCARD key` | 元素总数 | `zsl->length`，O(1) |

整个设计的核心洞察是：跳表 + 哈希表构成了一个**双向索引**。哈希表提供 O(1) 的成员查找（`ZSCORE`、`ZREM` 定位节点），跳表提供 O(log N) 的排序、排名和范围查询。两者共享同一份 SDS 字符串，不产生额外内存拷贝。

::: generate zset-architecture kind=explanation concepts=skiplist-struct,skiplist-node-struct

面向 C 学习者总结：Redis 有序集合的跳表实现虽然只有约 700 行 C 代码，但涵盖了多个重要的 C 编程技巧——柔性数组成员、单次分配嵌入数据、位域复用（level[0].span 存储元信息）、哨兵节点简化边界判断、span 数组将排名查询从 O(N) 优化到 O(log N)。这些技巧在各种高性能 C 项目中反复出现，值得仔细研读。

:::
