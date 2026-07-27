# 源码阅读：SQLite B 树与存储引擎

SQLite 把整个数据库存储在一个文件里。这个文件不是随意堆放的二进制块，而是一个由三层组件精密管理的结构。本文从顶向下拆解这三层：B 树负责组织数据的逻辑结构，Pager 负责页缓存和崩溃安全写入，WAL 负责高并发场景下的读写分离。

> 源码仓库：<https://sqlite.org/src>，Public Domain。本文基于 SQLite 3.46 版本。

## 1. 存储栈：三层分明

SQLite 的存储引擎是一个分层架构，每层只依赖下层的接口：

```
┌─────────────────────────────────┐
│         B-tree 层               │  组织数据：查找、插入、删除、遍历
│   btree.c / btreeInt.h          │
├─────────────────────────────────┤
│         Pager 层                │  页缓存 + 事务安全
│   pager.c                       │
├─────────────────────────────────┤
│         OS 层 (VFS)             │  文件读写、锁、同步
│   os_unix.c / os_win.c         │
└─────────────────────────────────┘
```

B 树通过 `sqlite3PagerGet()` 获取页面，通过 `sqlite3PagerWrite()` 标记脏页。Pager 通过 VFS 接口读写磁盘文件。每层的职责边界非常清晰——B 树不关心数据如何落盘，Pager 不关心页面里存的是什么。

在 WAL 模式下，Pager 和 WAL 模块协作：写入时脏页追加到 WAL 文件而不是直接修改数据库文件，读取时 WAL 模块决定应该从 WAL 还是数据库文件读取某页的数据。

## 2. 页式存储

::: concept page-storage title="页式存储"

整个数据库文件被划分为等大的页（page），默认 4096 字节。所有数据——B 树节点、溢出内容、空闲链表——都存在页里。页编号从 1 开始（0 表示"不存在"），第 1 页比较特殊，开头 100 字节是数据库文件头：

```
OFFSET   SIZE    DESCRIPTION
  0       16     魔数: "SQLite format 3\000"
 16        2     页大小 (字节)
 24        4     文件更改计数器
 28        4     数据库总页数
 32        4     第一个空闲链表页
 40       60     15 个 4 字节的元数据
```

页大小可以是 512 到 65536 之间的 2 的幂。一旦创建后通常不再改变。

:::

`BtShared` 是整个 B 树文件的核心句柄，它持有 Pager 指针和第 1 页的引用：

```c
struct BtShared {
  Pager *pPager;        /* 页缓存 */
  MemPage *pPage1;      /* 数据库第 1 页 */
  BtCursor *pCursor;    /* 所有打开的游标链表 */
  u32 pageSize;         /* 页大小 (字节) */
  u32 usableSize;       /* 每页可用字节数 */
  u16 maxLocal;         /* 非叶数据页的最大本地负载 */
  u16 minLocal;         /* 非叶数据页的最小本地负载 */
  u16 maxLeaf;          /* 叶数据页的最大本地负载 */
  u16 minLeaf;          /* 叶数据页的最小本地负载 */
  // ...
};
```

`usableSize = pageSize - nReserve`，预留字节给扩展（如加密）。B 树的空间计算都用 `usableSize`。

## 3. 两种 B 树

::: concept btree-types title="Table B-tree 与 Index B-tree"

SQLite 的每个表和每个索引各对应一棵独立的 B 树，它们共享同一个数据库文件，靠各自的根页号区分。B 树有两种类型：

| 类型 | 键 | 数据 | 用途 |
|:---|:---|:---|:---|
| Table B-tree | 64 位整数（rowid） | 存在叶节点 | 存储表的行数据 |
| Index B-tree | 任意长度的二进制键 | 不存数据（键即全部信息） | 存储索引 |

在源码中，区分靠页头标志位的组合：

```c
#define PTF_INTKEY    0x01   /* 键是整数 */
#define PTF_ZERODATA  0x02   /* 无数据，只有键 */
#define PTF_LEAFDATA  0x04   /* 数据只存在叶节点 */
#define PTF_LEAF      0x08   /* 叶节点（无子页指针） */
```

Table B-tree 的页标志是 `PTF_INTKEY | PTF_LEAFDATA | PTF_LEAF`（叶节点）或 `PTF_INTKEY | PTF_LEAFDATA`（内部节点）。Index B-tree 的页标志是 `PTF_ZERODATA | PTF_LEAF`（叶节点）或 `PTF_ZERODATA`（内部节点）。

:::

::: generate btree-vs-bplus kind=comparison concepts=btree-types

面向有数据结构基础的 C 程序员，比较经典 B 树和 SQLite 的 B+ 树变体：经典 B 树在每个节点都存数据，而 SQLite 的 Table B-tree 只在叶节点存数据、内部节点只存键和子指针（这正是 B+ 树的特征）；但 Index B-tree 在每个节点都存键且无额外数据，更接近经典 B 树。指出 SQLite 根据用途选择不同变体是一个务实的工程决策。

:::

## 4. 页面格式

::: concept page-format title="B 树页面的内部布局"

每个 B 树页面由四个区域组成，从低地址到高地址依次是：

```
|----------------|
| 页头           |   叶节点 8 字节，内部节点 12 字节
|----------------|
| 单元格指针数组  |   每个 2 字节，按键排序，向下增长
|                |         ↓
|----------------|
| 未分配空间      |
|----------------|         ↑
| 单元格内容区    |   存放实际数据，向上增长，顺序任意
|                |
|----------------|
```

页头的结构：

```
OFFSET   SIZE     DESCRIPTION
  0       1      标志位 (PTF_INTKEY, PTF_LEAF 等)
  1       2      第一个空闲块的偏移
  3       2      本页单元格数量
  5       2      单元格内容区起始位置
  7       1      碎片字节总数
  8       4      最右子页指针 (仅内部节点)
```

单元格指针数组是有序的（按键排序），但单元格内容区的存储顺序是任意的。这个设计让插入操作只需在指针数组中插入 2 字节并追加内容，无需移动已有的单元格数据。

:::

`MemPage` 是一个 B 树页面被加载到内存后的表示：

```c
struct MemPage {
  u8 isInit;           /* 是否已初始化 */
  u8 intKey;           /* True: table b-tree; False: index b-tree */
  u8 intKeyLeaf;       /* True: intkey 表的叶节点 */
  u8 leaf;             /* True: 叶节点 */
  u8 hdrOffset;        /* 页头偏移 (第 1 页为 100，其他为 0) */
  u8 childPtrSize;     /* leaf ? 0 : 4 */
  u16 nCell;           /* 本页单元格数量 */
  u16 cellOffset;      /* 单元格指针数组在 aData 中的偏移 */
  int nFree;           /* 空闲字节数 */
  u8 *aData;           /* 指向页面原始数据 */
  u8 *aCellIdx;        /* 单元格指针数组起始位置 */
  BtShared *pBt;       /* 所属的 BtShared */
  DbPage *pDbPage;     /* Pager 层的页面句柄 */
  // ...
};
```

注意 `hdrOffset`：第 1 页的前 100 字节是文件头，页头从偏移 100 开始；其他页从 0 开始。

每个单元格由可选的左子页号（4 字节）、varint 编码的数据/键长度、负载数据、可选的溢出页号组成。varint 是 SQLite 的变长整数编码：每字节低 7 位是数据，最高位标记是否有后续字节，最多 9 字节可编码 64 位整数。

## 5. BtCursor：定位迭代器

对 B 树的所有访问都通过游标（`BtCursor`）进行。游标指向 B 树中的一个特定单元格，支持定位（MoveTo）、步进（Next/Previous）、读取和修改。

```c
struct BtCursor {
  u8 eState;            /* CURSOR_VALID, CURSOR_INVALID 等 */
  Btree *pBtree;        /* 所属连接 */
  BtShared *pBt;        /* 所属文件 */
  Pgno pgnoRoot;        /* B 树根页号 */
  i64 nKey;             /* 最后访问的整数键 */
  CellInfo info;        /* 当前单元格的解析结果 */
  MemPage *pPage;                        /* 当前页 */
  MemPage *apPage[BTCURSOR_MAX_DEPTH-1]; /* 祖先页栈 */
  u16 ix;               /* 当前页中的单元格索引 */
  u16 aiIdx[BTCURSOR_MAX_DEPTH-1];       /* 祖先页中的索引 */
  i8 iPage;             /* apPage 栈的当前深度 */
  // ...
};
```

`pPage` + `ix` 确定了游标在树中的精确位置，`apPage[]` + `aiIdx[]` 记录了从根到当前页的完整路径——这使得向上回溯（`moveToParent`）成为 O(1) 操作。

### 5.1 定位：sqlite3BtreeTableMoveto

在 Table B-tree 中查找一个整数键，核心逻辑是从根页开始的逐层二分搜索：

```c
int sqlite3BtreeTableMoveto(BtCursor *pCur, i64 intKey,
                            int biasRight, int *pRes) {
  // 优化：如果游标已经在目标位置，直接返回
  if( pCur->eState==CURSOR_VALID && (pCur->curFlags & BTCF_ValidNKey)!=0 ){
    if( pCur->info.nKey==intKey ){
      *pRes = 0;
      return SQLITE_OK;
    }
    // 优化：如果目标键是当前键 +1，尝试 Next 而非全量搜索
    if( pCur->info.nKey+1==intKey ){
      rc = sqlite3BtreeNext(pCur, 0);
      // ...
    }
  }

  rc = moveToRoot(pCur);  // 移动到根页
  for(;;){
    MemPage *pPage = pCur->pPage;
    int lwr = 0, upr = pPage->nCell - 1;

    // 页内二分搜索
    for(;;){
      int idx = (lwr + upr) / 2;
      i64 nCellKey;
      pCell = findCellPastPtr(pPage, idx);
      getVarint(pCell, (u64*)&nCellKey);

      if( nCellKey < intKey )      lwr = idx + 1;
      else if( nCellKey > intKey ) upr = idx - 1;
      else { /* 找到了 */ *pRes = 0; return SQLITE_OK; }

      if( lwr > upr ) break;  // 本页没有精确匹配
    }
    if( pPage->leaf ) break;          // 叶节点，搜索结束
    moveToChild(pCur, childPageNo);   // 进入子页，继续搜索
  }
}
```

两个值得注意的优化：(1) 如果游标已经在正确位置就直接返回；(2) 如果目标键是当前键 +1（这在顺序插入时非常常见），先尝试 `sqlite3BtreeNext()` 而不是从根重新搜索。

### 5.2 步进：sqlite3BtreeNext

`sqlite3BtreeNext()` 把游标移到下一个单元格。快路径极简——只是递增索引：

```c
int sqlite3BtreeNext(BtCursor *pCur, int flags){
  MemPage *pPage;
  pCur->info.nSize = 0;
  pCur->curFlags &= ~(BTCF_ValidNKey|BTCF_ValidOvfl);
  if( pCur->eState!=CURSOR_VALID ) return btreeNext(pCur);
  pPage = pCur->pPage;
  if( (++pCur->ix) >= pPage->nCell ){
    pCur->ix--;
    return btreeNext(pCur);  // 慢路径：需要切换页面
  }
  if( pPage->leaf ){
    return SQLITE_OK;         // 快路径：叶节点上向右移一格
  }else{
    return moveToLeftmost(pCur); // 内部节点：进入子树最左叶
  }
}
```

慢路径 `btreeNext()` 处理跨页的情况：叶节点遍历完后通过 `moveToParent()` 沿 `apPage[]` 栈向上回溯到父节点，然后再下到下一个子树的最左叶——标准的 B 树中序遍历。到达根页仍无法前进时返回 `SQLITE_DONE`。

## 6. 插入与分裂

### 6.1 sqlite3BtreeInsert

插入操作的入口是 `sqlite3BtreeInsert()`，它接收一个 `BtreePayload` 结构体（包含键和数据），将其写入游标指向的位置：

```c
int sqlite3BtreeInsert(
  BtCursor *pCur,
  const BtreePayload *pX,  /* 要插入的行 */
  int flags,
  int seekResult
){
  // 1. 保存其他游标的位置（它们可能被打乱）
  if( pCur->curFlags & BTCF_Multiple ){
    saveAllCursors(p->pBt, pCur->pgnoRoot, pCur);
  }

  // 2. 如果是 table b-tree 且键已存在且大小相同：直接覆写
  if( pCur->info.nPayload == newPayloadSize ){
    return btreeOverwriteCell(pCur, pX);
  }

  // 3. 组装新单元格，插入到当前页
  // 4. 如果页溢出，调用 balance() 重新平衡
  rc = balance(pCur);
}
```

覆写优化很重要：当 UPDATE 语句修改一行但新数据和旧数据大小相同时，直接原地覆写，完全跳过了删除旧单元格、插入新单元格、可能触发分裂的整个流程。

### 6.2 balance()：分裂与重分配

当一个页面溢出（单元格放不下了）或严重不足（空闲空间超过 2/3）时，`balance()` 负责重新分配。它是一个循环——因为重分配可能导致父页也溢出，需要逐层向上处理：

```c
static int balance(BtCursor *pCur){
  do {
    MemPage *pPage = pCur->pPage;

    // 不需要平衡的条件：无溢出且空闲空间 < 2/3
    if( pPage->nOverflow==0
     && pPage->nFree*3 <= (int)pCur->pBt->usableSize*2 ){
      break;
    }

    if( pCur->iPage==0 ){
      // 根页溢出 → balance_deeper(): 创建新子页，根页上升一层
      rc = balance_deeper(pPage, &pCur->apPage[1]);
    }else{
      if( /* 快速路径条件：intkey 叶节点，单个溢出在末尾 */ ){
        // balance_quick(): 创建一个新兄弟页，把溢出的单元格移过去
        rc = balance_quick(pParent, pPage, aBalanceQuickSpace);
      }else{
        // balance_nonroot(): 在当前页和最多 2 个兄弟页之间重分配
        rc = balance_nonroot(pParent, iIdx, ...);
      }
    }
    // 向上移动游标，下一轮循环检查父页
    pCur->pPage = pCur->apPage[--pCur->iPage];
  } while( rc==SQLITE_OK );
}
```

三个平衡例程各有分工：

- **`balance_deeper()`**：仅用于根页溢出。创建一个新子页，把根页内容复制下去，根页变成只有一个子指针的内部节点。这是 B 树长高的唯一方式。
- **`balance_quick()`**：快速路径优化。当 intkey 叶节点的溢出单元格恰好在末尾（顺序插入的典型场景），直接创建一个新兄弟页接收它，不做复杂的重分配。
- **`balance_nonroot()`**：通用路径。收集当前页和最多 2 个兄弟页的所有单元格，重新均匀分配到（可能更多的）页中。

::: generate page-split kind=example concepts=page-format,btree-types

面向有数据结构基础的 C 程序员，用一个具体的数字例子逐步走完一次叶节点分裂的过程：假设页大小很小（容纳 3 个单元格），依次插入键 10, 20, 30, 40，在插入 40 时触发分裂。说明分裂前后页面的内容变化，父节点如何获得一个新的分隔键，以及如果父节点也满了会发生什么（级联分裂直到根）。

:::

## 7. Pager 层：页缓存与崩溃安全

::: concept pager-state-machine title="Pager 状态机"

Pager 用一个状态机管理事务生命周期：

```
OPEN → READER → WRITER_LOCKED → WRITER_CACHEMOD → WRITER_DBMOD → WRITER_FINISHED → READER
                                                                                      ↑
任何 WRITER 状态 ————→ ERROR ————→ OPEN ————————————————————————————————————————————————┘
```

关键状态：

| 状态 | 含义 |
|:---|:---|
| OPEN | 初始状态，未持有锁 |
| READER | 持有 SHARED 锁，可读不可写 |
| WRITER_LOCKED | 持有 RESERVED 锁，还未修改任何数据 |
| WRITER_CACHEMOD | 缓存中的页已被修改，日志文件已打开 |
| WRITER_DBMOD | 修改已写入数据库文件 |
| WRITER_FINISHED | 数据库写入完毕，等待提交（删除日志） |
| ERROR | 出现 I/O 错误，拒绝一切操作直到回滚 |

在 WAL 模式下，Pager 不会进入 WRITER_DBMOD 和 WRITER_FINISHED 状态——因为修改写入 WAL 而非数据库文件。

:::

`Pager` 结构体持有数据库文件描述符 `fd`、日志文件描述符 `jfd`、页缓存 `pPCache`、WAL 句柄 `pWal`，以及一个 `Bitvec *pInJournal` 位图记录哪些页已写入日志。`eState` 和 `eLock` 分别跟踪状态机位置和文件锁级别。

### 7.1 获取页面：sqlite3PagerGet

B 树层通过 `sqlite3PagerGet(pPager, pgno, &pPage, flags)` 获取页面。内部通过函数指针 `pPager->xGet` 分发——WAL 模式下指向 `pagerGetWal()`，它先在 WAL 中查找该页最新版本，找不到才读数据库文件。

### 7.2 标记脏页：sqlite3PagerWrite / pager_write

当 B 树要修改一个页面时，必须先调用 `sqlite3PagerWrite()` 标记它为"可写"：

```c
static int pager_write(PgHdr *pPg){
  Pager *pPager = pPg->pPager;

  // 如果还没打开日志文件，先打开
  if( pPager->eState==PAGER_WRITER_LOCKED ){
    rc = pager_open_journal(pPager);
  }

  // 标记为脏页
  sqlite3PcacheMakeDirty(pPg);

  // 回滚日志模式下：把原始内容写入日志（如果还没写过）
  if( pPager->pInJournal!=0
   && sqlite3BitvecTestNotNull(pPager->pInJournal, pPg->pgno)==0 ){
    if( pPg->pgno <= pPager->dbOrigSize ){
      rc = pagerAddPageToRollbackJournal(pPg);  // 写入日志
    }
  }

  // 设置 PGHDR_WRITEABLE 标志，允许上层修改页面内容
  pPg->flags |= PGHDR_WRITEABLE;
  return rc;
}
```

关键原则：**先写日志，再改数据**。在修改页面之前，必须先把页面的原始内容写入回滚日志。这样崩溃后可以从日志恢复。

### 7.3 崩溃恢复：pager_playback

::: concept rollback-journal title="回滚日志的崩溃恢复"

回滚日志的恢复逻辑（`pager_playback()`）非常直接：逐条读取日志中保存的原始页面，写回数据库文件，撤销所有未提交的修改：

1. 打开日志文件，读取头信息（原始数据库大小、记录数）
2. 逐条读取日志记录（每条是一个完整页面）
3. 把每个页面写回数据库文件的原始位置
4. 截断数据库文件到原始大小
5. 删除日志文件

日志删除是原子提交点——如果日志还在，说明事务未完成，需要回滚；如果日志不在了，说明事务已经提交。

:::

::: generate journal-vs-wal kind=comparison concepts=rollback-journal,pager-state-machine

面向有数据结构基础的 C 程序员，比较回滚日志（rollback journal）和 WAL 两种事务机制：回滚日志在写入前保存原始页面，崩溃后撤销修改（undo log）；WAL 把新版本追加到 WAL 文件，崩溃后丢弃未提交的 WAL 帧（redo log）。对比两者在写入放大、读并发、恢复速度方面的差异，解释为什么 WAL 模式在大多数场景下性能更好。

:::

## 8. WAL 模式

### 8.1 核心思路

::: concept wal-protocol title="WAL 协议"

WAL（Write-Ahead Log）模式的核心思路：不修改数据库文件本身，而是把新版本的页面追加到一个单独的 WAL 文件中。

- **写入**：脏页写到 WAL 文件末尾，每个页面组成一个"帧"（frame），包含帧头（页号、数据库大小、校验和）和页面数据。
- **读取**：先在 WAL 中查找该页的最新帧，如果找到则读 WAL，否则读数据库文件。
- **提交**：写入一个提交帧（`nTruncate > 0`），事务即完成。不需要 fsync 数据库文件。
- **检查点**：定期把 WAL 中已提交的页面回写到数据库文件，然后 WAL 可以复用。

:::

`Wal` 结构体持有数据库文件和 WAL 文件的描述符、三种锁的状态（`readLock`、`writeLock`、`ckptLock`）、当前事务快照 `hdr`（类型 `WalIndexHdr`），以及 WAL 索引的共享内存映射 `apWiData`。

### 8.2 读写不阻塞

读事务的开启（`walTryBeginRead`）会记录当前 WAL 的进度——`mxFrame`，此后读者只看到这个快照：

```c
// walTryBeginRead 的核心逻辑 (简化)
// 1. 读取 WAL 索引头，获得当前 mxFrame
// 2. 选择一个读锁槽位，锁定之
// 3. 记录 aReadMark[readLock] = mxFrame
```

写事务通过 `walFrames()` 把脏页追加到 WAL：

```c
static int walFrames(Wal *pWal, int szPage,
                     PgHdr *pList, Pgno nTruncate,
                     int isCommit, int sync_flags) {
  // 如果 WAL 是空的，先写 WAL 文件头
  if( pWal->hdr.mxFrame==0 ){
    // 写入 32 字节的 WAL 头：魔数、版本、页大小、salt、校验和
    rc = sqlite3OsWrite(pWal->pWalFd, aWalHdr, sizeof(aWalHdr), 0);
  }

  // 逐页写入帧
  for(p=pList; p; p=p->pDirty){
    iFrame++;
    // 写帧头 (24 字节): 页号, nTruncate(提交帧), salt, 校验和
    // 写页面数据
    rc = walWriteOneFrame(&w, p, nTruncate, iOffset);
    iOffset += szFrame;

    // 更新 WAL 索引中的哈希表
  }

  // 如果是提交，同步 WAL 文件
  if( isCommit && sync_flags ){
    rc = sqlite3OsSync(pWal->pWalFd, sync_flags);
  }

  // 更新共享的 WAL 索引头 (原子地更新 mxFrame)
}
```

::: generate wal-readers-writers kind=explanation concepts=wal-protocol

面向有数据结构基础的 C 程序员，解释 WAL 模式如何实现"读者不阻塞写者、写者不阻塞读者"：每个读者在开始读事务时记录当前 WAL 的 mxFrame（最大帧号），之后只看到该帧号及之前的数据，后续写者追加的帧对其不可见。写者只需获取排他的"写锁"来防止多个并发写入，但不需要等待读者完成。解释 wal-index（共享内存中的哈希表）如何让读者快速查找某页在 WAL 中的最新帧。

:::

### 8.3 检查点

检查点（checkpoint）把 WAL 中已提交的帧回写到数据库文件。`sqlite3WalCheckpoint()` 先获取排他的检查点锁（同时只能有一个检查点），然后根据模式决定是否还需要获取写锁。四种模式：

| 模式 | 行为 |
|:---|:---|
| PASSIVE | 尽量回写，不阻塞读写，可能只完成部分 |
| FULL | 等待写者完成，回写所有帧，但不等待读者释放 WAL |
| RESTART | 回写所有帧，等待所有读者离开 WAL，重置 WAL 到开头 |
| TRUNCATE | 同 RESTART，并截断 WAL 文件为零 |

回写顺序很讲究：按页号而非帧顺序回写，使数据库文件的写入是顺序的，磁盘效率更高。`WalIterator` 负责按页号遍历 WAL 中需要回写的帧。

### 8.4 WAL 读取路径

读页时先在 WAL 索引哈希表中查找页号，如果找到帧且帧号 <= 当前读事务的 `mxFrame`，就从 WAL 文件读取（`sqlite3WalReadFrame` 根据帧号计算文件偏移直接读取）；否则从数据库文件读取。

## 9. 总结

SQLite 的存储引擎是一个教科书级别的分层设计：

- **B 树层**把数据组织为页大小的节点，用游标提供定位和遍历接口，通过 `balance()` 保持树的平衡。
- **Pager 层**用页缓存加速访问，用回滚日志（或 WAL）保证崩溃安全，用文件锁管理并发。
- **WAL 模块**通过追加写和快照读实现读写分离，用共享内存中的索引加速帧查找。

每一层都可以独立理解，组合在一起构成了一个可靠、高效、跨平台的嵌入式数据库引擎。理解这套架构后，再去读 SQLite 的其他部分（VDBE 虚拟机、查询优化器）就有了坚实的底座。
