# 6.2 源码阅读：SQLite 虚拟机（VDBE）

在很多人的直觉里，嵌入式数据库也许会像简单的脚本解释器一样，一边遍历 SQL 语法树，一边去磁盘文件里查找数据。但 SQLite 并未采取这种直接解释的做法，而是选择了一条更为经典的系统架构路线：先把声明式的 SQL 文本编译成紧凑的底层字节码，随后交给一台专用的虚拟机逐条调度执行。

这台虚拟机在 SQLite 内部被称为 VDBE（Virtual Database Engine，虚拟数据库引擎）。它是整个系统最关键的执行中枢。通过在客户端中执行 `EXPLAIN` 前缀，我们可以直接窥探一条查询被翻译后的真实指令序列（以 `CREATE TABLE users(id INTEGER PRIMARY KEY, name TEXT, age INTEGER)` 为例）：

```sql
sqlite> EXPLAIN SELECT name FROM users WHERE age > 30;
addr  opcode         p1    p2    p3    p4             p5
----  -------------  ----  ----  ----  -------------  --
0     Init           0     9     0                    0
1     OpenRead       0     2     0     3              0
2     Rewind         0     8     0                    0
3       Column       0     2     1                    0
4       Le           2     7     1     BINARY-8       84
5       Column       0     1     3                    0
6       ResultRow    3     1     0                    0
7     Next           0     3     0                    1
8     Halt           0     0     0                    0
9     Transaction    0     0     1     0              1
10    Integer        30    2     0                    0
11    Goto           0     1     0                    0
```

这种“先编译、后由虚拟机执行”的设计范式在编程语言运行时（例如 Lua、CPython 或 JVM）中十分普遍。不同之处在于，通用语言虚拟机的操作码大多围绕函数调用、栈帧管理和通用运算展开；而 VDBE 的指令集则是专门为关系型数据库定制的领域专用指令集，原生包含开启事务、寻址 B 树游标、提取列数据以及向上层返回结果行等操作。

在上一节的[架构概述](./6_1_架构概述.md)中，我们看到 VDBE 稳稳坐落在编译器前端与底层存储引擎之间。前端负责将语法推导与查询计划优化沉淀为线性指令流，VDBE 则在执行这些指令时调用 B-tree 和 Pager 层的 API 进行物理数据的检索与更新。接下来，我们将深入拆解 VDBE 自身的指令表示、内存模型与调度循环。

::: generate vdbe-why-vm kind=explanation concepts=vdbe-bytecode-format

面向正在学习 C 语言的读者，解释为什么 SQLite 选择"先编译后解释"的架构而不是直接解释 SQL。要点：(1) SQL 是声明式语言，不能直接执行；(2) 字节码提供了稳定的内部表示，把复杂的 SQL 优化（查询计划选择、索引使用决策）集中在编译阶段，执行阶段只需简单的指令分发；(3) 同一条预编译语句可以绑定不同参数反复执行，避免重复编译。类比 Python 的 .pyc 文件或 Java 的 .class 文件帮助读者理解。

:::

## 1. 字节码格式：VdbeOp 结构体

在 VDBE 的世界里，每一条字节码指令都由一个结构体 `VdbeOp`（源码中常简写为 `Op`）完整描述。它使用固定宽度的字段来记录当前操作的所有参数：

::: concept vdbe-bytecode-format title="字节码指令的格式"

```c
struct VdbeOp {
  u8 opcode;          /* 操作码：OP_Integer, OP_Column, ... */
  signed char p4type; /* p4 的类型标签 */
  u16 p5;             /* 第五操作数，16 位标志位 */
  int p1;             /* 第一操作数（通常是寄存器号或游标号） */
  int p2;             /* 第二操作数（常用作跳转目标地址） */
  int p3;             /* 第三操作数 */
  union p4union {     /* 第四操作数，类型由 p4type 决定 */
    int i;            /* 整数 */
    char *z;          /* 字符串 */
    FuncDef *pFunc;   /* 函数定义 */
    KeyInfo *pKeyInfo;/* 索引键信息 */
    CollSeq *pColl;   /* 排序规则 */
    Mem *pMem;        /* 默认值 */
    /* ... 其他变体 ... */
  } p4;
};
```

`p1`、`p2`、`p3` 是三个基础的整型操作数，它们的具体含义完全取决于当前操作码的要求。`p4` 是一个多态联合体，专门用来传递静态字符串、自定义函数指针、键排序规则等复杂对象。`p5` 则通常作为辅助标记或位掩码，为指令提供更精细的行为控制。

:::

这种全定长指令的设计，展现了 SQLite 在工程实现上的务实取舍。诸如 x86 这样的硬件指令集采用变长编码，解码引擎必须先解析操作码和前缀才能获知后续操作数的长度；而在 VDBE 里，所有指令在内存中都占有完全一致的字节大小。这意味着指令序列可以直接存储在一个连续的数组中，虚拟机推进下一条指令或实施跳转时，只需计算数组下标偏移即可，不仅消除了指令解码开销，还能极大提升 CPU 缓存的局部性命中率。

截至当前版本，SQLite 共定义了约 190 种操作码（可在自动生成的头文件 `opcodes.h` 中查阅），全面覆盖了算术运算、比较跳转、游标寻址、聚合统计以及底层事务控制。

## 2. 寄存器：Mem 结构体

不同于基于操作数栈（Stack-based）的虚拟机，VDBE 是一台**基于寄存器**（Register-based）的虚拟机。在 VDBE 的设计哲学中，寄存器并不是 CPU 物理寄存器的映射，而是指一个由虚拟数据单元组成的线性数组。

每个寄存器在底层都由一个 `Mem` 结构体（对外以不透明类型 `sqlite3_value` 暴露）来承载。它本质上是一个经典的**带类型标签的联合体**（tagged union），能够以统一的内存容器容纳 SQL 所支持的任意五种基本类型（NULL、整数、浮点数、字符串、二进制 BLOB）：

::: concept vdbe-mem-struct title="寄存器是带类型标签的联合体"

```c
struct sqlite3_value {
  union MemValue {
    double r;           /* 浮点值 */
    i64 i;              /* 整数值 */
    int nZero;          /* BLOB 末尾的零填充字节数 */
    const char *zPType; /* 指针类型标识 */
    FuncDef *pDef;      /* 聚合函数上下文 */
  } u;
  char *z;              /* 字符串或 BLOB 的数据 */
  int n;                /* 字符串长度（不含 '\0'） */
  u16 flags;            /* 类型标签：MEM_Null, MEM_Int, ... */
  u8 enc;               /* 字符编码 */
  sqlite3 *db;          /* 所属的数据库连接 */
  int szMalloc;         /* 动态分配的缓冲区大小 */
  char *zMalloc;        /* 动态分配的缓冲区 */
  void (*xDel)(void*);  /* 析构函数 */
};
```

类型标签 `flags` 决定了当前如何正确解释联合体内的原始比特：

```c
#define MEM_Null      0x0001   /* NULL */
#define MEM_Str       0x0002   /* 字符串 */
#define MEM_Int       0x0004   /* 整数 */
#define MEM_Real      0x0008   /* 浮点数 */
#define MEM_Blob      0x0010   /* 二进制大对象 */
```

:::

在 `Mem` 的布局中，整型和浮点型这类固定长度的值直接安放在联合体 `u` 内部，彼此共享 8 字节空间；而对于长度不定的文本和 BLOB 数据，寄存器内部无法静态容纳，因而通过指针 `z` 指向外部的连续内存缓冲区。

为了精细管理内存的所有权，`flags` 的高位还编码了 `z` 所指向缓冲区的生命周期属性：

```c
#define MEM_Dyn       0x1000   /* z 指向动态分配的堆内存，释放时必须调用 xDel */
#define MEM_Static    0x2000   /* z 指向静态常量区（例如编译期字面量），无需回收 */
#define MEM_Ephem     0x4000   /* z 指向临时数据（如磁盘页缓存），随时可能失效 */
```

这三个内存归属标志是 SQLite 防止内存泄漏与悬垂指针的核心机制。当某个操作码准备将新数据写入一个已占用的寄存器时，虚拟机会先检查其旧有的 `flags`；若检测到 `MEM_Dyn`，便会触发析构回调 `xDel(z)` 清理旧堆内存；而若仅仅是 `MEM_Static` 或 `MEM_Ephem`，则说明当前寄存器并不拥有该内存块的所有权，可以直接覆盖指针而不必做额外释放。

::: generate vdbe-register-vs-stack kind=comparison concepts=vdbe-mem-struct

面向正在学习 C 语言的读者，对比基于寄存器的虚拟机和基于栈的虚拟机。要点：(1) 栈式 VM（如 JVM、CPython）用 push/pop 传递操作数，指令短但数量多；(2) 寄存器式 VM（如 Lua 5、VDBE）在指令中直接编码寄存器号，指令长但数量少；(3) VDBE 选择寄存器式的原因：数据库操作经常需要同时持有多个列值、比较结果和游标状态，用寄存器可以避免大量冗余的 push/pop。用一个简单的加法 `a + b` 的例子对比两种方式的指令序列。

:::

## 3. 游标：VdbeCursor 结构体

如果说寄存器是虚拟机处理算术和逻辑的数据容器，那么**游标**（Cursor）就是 VDBE 穿透到存储引擎底层的 I/O 窗口。

每一条需要读写磁盘数据的查询，都会按需打开一个或多个游标。在运行时，每个游标对应一个 `VdbeCursor` 结构体，代表一个“当前打开并定位在某一条记录上的表或索引”：

```c
struct VdbeCursor {
  u8 eCurType;            /* CURTYPE_BTREE, CURTYPE_SORTER, ... */
  u8 nullRow;             /* 1 = 当前没有指向有效行 */
  u8 isTable;             /* 1 = 表 B-tree（按 rowid 组织） */
  union {
    BtCursor *pCursor;    /* B-tree 游标 */
    sqlite3_vtab_cursor *pVCur; /* 虚拟表游标 */
    VdbeSorter *pSorter;  /* 排序器 */
  } uc;
  u32 *aOffset;           /* 各列在记录中的偏移量（缓存） */
  const u8 *aRow;         /* 当前行的原始数据 */
  u16 nHdrParsed;         /* 已解析的头部字段数 */
  u32 aType[FLEXARRAY];   /* 各列的 serial type（柔性数组） */
};
```

结构体末尾的 `aType` 采用了 C99 标准引入的**柔性数组成员**（flexible array member）。在分配游标空间时，`allocateCursor` 会根据目标表的实际列数计算出额外的内存空间，只通过一次 `malloc` 就能将 `VdbeCursor` 结构体头和整个列类型数组分配在一块连续内存中。这不仅去除了二次间接指针寻址，还大幅降低了高频分配与碎片化的压力。

游标还承担了行记录的解析缓存职责。由于 SQLite 的磁盘行数据采用紧凑的序列化编码，字段偏移量不是固定的，`VdbeCursor` 维护了 `aOffset` 与 `nHdrParsed`，确保在同一次 `step` 中提取多列时，无须反复解析同一行的头部字节。

## 4. 编译后的程序：Vdbe 结构体

现在，执行程序所需的要素已经全部就绪：指令序列（`Op[]`）构成代码段，寄存器阵列（`Mem[]`）构成数据区，游标列表（`VdbeCursor*[]`）构成存储访问通道。把这三者编织在一起并统筹调度的，正是整个虚拟机的主控结构体 `Vdbe`。

::: concept vdbe-struct title="Vdbe 结构体是编译后的完整程序"

`Vdbe` 结构体将执行一条 SQL 语句所需的所有运行时上下文完整封装起来：

```c
struct Vdbe {
  sqlite3 *db;            /* 所属的数据库连接 */
  Op *aOp;                /* 指令数组——字节码程序本身 */
  int nOp;                /* 指令条数 */
  Mem *aMem;              /* 寄存器数组 */
  int nMem;               /* 寄存器个数 */
  VdbeCursor **apCsr;     /* 游标数组（每个打开的 B-tree 对应一个） */
  int nCursor;            /* 游标槽位数 */
  int pc;                 /* 程序计数器——当前执行到第几条指令 */
  int rc;                 /* 最近一次操作的返回码 */
  Mem *pResultRow;        /* 指向当前结果行的起始寄存器 */
  u16 nResColumn;         /* 结果行的列数 */
  u8 eVdbeState;          /* INIT / READY / RUN / HALT */
  char *zSql;             /* 原始 SQL 文本（调试用） */
  /* ... */
};
```

整个运行时的动态状态全部落入这三组数组之中：

| 数组 | 类型 | 作用 |
|:---|:---|:---|
| `aOp[0..nOp-1]` | `Op` | 字节码指令序列（只读执行） |
| `aMem[0..nMem-1]` | `Mem` | 寄存器阵列（计算与临时数据存储） |
| `apCsr[0..nCursor-1]` | `VdbeCursor*` | B 树游标句柄（底层存储寻址与遍历） |

:::

站在外部 API 的视角，`sqlite3_prepare_v2()` 负责在堆上构建并初始化这样一个 `Vdbe` 实例（以不透明指针 `sqlite3_stmt *` 交给调用者）；后续每次调用 `sqlite3_step()`，都是在驱动这个状态机向前推进；最终的 `sqlite3_finalize()` 则是一次性析构并释放全部相关资源。

`eVdbeState` 字段精确记录了该虚拟机的生命演进：

```c
#define VDBE_INIT_STATE     0   /* 正在构建中（prepare 阶段） */
#define VDBE_READY_STATE    1   /* 准备就绪，尚未开始执行 */
#define VDBE_RUN_STATE      2   /* 正在执行中 */
#define VDBE_HALT_STATE     3   /* 已结束，需要 reset 或 finalize */
```

这是一个极其健壮的状态机模型。`sqlite3_step()` 仅在虚拟机处于 READY 或 RUN 状态时才会触发真正的核心执行函数 `sqlite3VdbeExec()`；一旦指令执行完毕或者被终止，状态即转入 HALT。若调用方希望重复执行该语句，调用 `sqlite3_reset()` 即可将程序计数器 `pc` 回退、清空临时寄存器并将状态重置为 READY，使预编译的字节码可以在新的循环中反复投产。

## 5. 执行引擎：sqlite3VdbeExec

当外部调用 `sqlite3_step` 时，执行流程最终会汇聚到 `vdbeaux.c` 与 `vdbe.c` 中的执行核心：`sqlite3VdbeExec`。

这是一个在开源界罕见而又极具特色的函数——单个函数体长达近 9000 行，内部以一个庞大的 `switch` 结构容纳了 180 多个分支。这里没有抽象的函数指针分发数组，也没有任何间接的面向对象调用，有的只是最纯粹、最直白的指令调度循环。

::: concept vdbe-dispatch-loop title="指令分发循环的结构"

VDBE 的调度循环遵循了经典而高效的直接分发模式：外层由一个死循环或遍历指令数组的 `for` 语句驱动，内层则直接以操作码作为分发键，由编译器根据 `switch` 生成密集的跳转表（jump table）。指令指针在每次循环中自增推进，而跳转类操作码则通过直接重置指针位置改变控制流。

:::

### 5.1 主循环结构

剥离掉庞杂的各操作码分支后，整个执行函数的主干轮廓非常清晰：

```c
int sqlite3VdbeExec(Vdbe *p) {
  Op *aOp = p->aOp;          /* 指令数组（本地缓存，避免重复解引用） */
  Op *pOp = aOp;             /* 当前指令指针 */
  Mem *aMem = p->aMem;       /* 寄存器数组 */
  int rc = SQLITE_OK;

  Mem *pIn1, *pIn2, *pIn3;   /* 输入寄存器的快捷指针 */
  Mem *pOut;                  /* 输出寄存器的快捷指针 */

  for(pOp = &aOp[p->pc]; 1; pOp++){
    switch( pOp->opcode ){
      case OP_Goto: { /* ... */ break; }
      case OP_Integer: { /* ... */ break; }
      /* ... 180 多个 case ... */
    }
  }
}
```

在工程实现上，这个看似原始的巨大循环藏着几个至关重要的性能权衡：

**跳转表分发与分支预测**：现代编译器在处理 case 密集且连续的 `switch` 时，会将其编译为一张连续的地址跳转表，每次操作码分发仅需一次寄存器相对寻址的间接跳转。在支持 GNU C 扩展的平台上，SQLite 甚至支持直接线程化代码（Direct Threading，利用 `&&label` 数组），彻底消除了每次循环尾部的中转开销。

**自增推进与预减跳转**：指令指针 `pOp++` 直接安放在 `for` 循环头部。每条常规指令在自身逻辑结束后直接 `break`，即可自动步进到下一条指令，完全省去了在 180 多个分支里反复书写 `pc++` 的冗余。反之，当遇到需要跳转的指令时，虚拟机把目标指针设定为 `&aOp[target - 1]`（之所以减 1，是为了抵消下一轮循环头部的递增），随后执行 `break`。源码中为这种跳转模式提取了公共标签：

```c
jump_to_p2:
  pOp = &aOp[pOp->p2 - 1];
  break;

jump_to_p2_and_check_for_interrupt:
  pOp = &aOp[pOp->p2 - 1];
  /* 检查 sqlite3_interrupt() 和进度回调 */
  break;
```

**可重入的暂停与恢复机制**：对于查询语句，遇到满足条件的行时会执行 `OP_ResultRow` 指令。该指令并不会执行 `break`，而是把下一条待执行指令的偏移存回 `p->pc`，随后通过 `goto vdbe_return` 直接退出 `sqlite3VdbeExec`，向外部返回状态码 `SQLITE_ROW`。上层应用拉取完当前行数据后，下一次调用 `sqlite3_step`，函数便能无缝从先前保存的 `p->pc` 处恢复循环，具备了类似协程（Coroutine）的流式生成特性。

### 5.2 局部对象缓存

在函数入口处，`sqlite3VdbeExec` 会优先把 `p->aOp` 与 `p->aMem` 解引用并缓存在局部的指针对象 `aOp` 和 `aMem` 中。在一个可能会循环数百万次的执行热点中，如果每次访问寄存器或指令都要穿透 `p` 指针进行多级解引用，不仅会产生昂贵的数据依赖，还会阻碍编译器的寄存器分配优化。将基础指针提升为局部对象后，C 编译器能够将其直接锁定在物理机器的寄存器中，内层循环可以直接以基址加偏移的方式高速存取。

## 6. 代表性指令拆解

为了透彻理解这些操作码如何在实践中彼此协同，我们可以选取其中最具代表性的六组指令进行逐项剖析。

### 6.1 OP_Integer：加载整型常量

这是整个指令集里最为精简的一类操作，主要负责把一个 32 位整型常量写入指定的寄存器槽位：

```c
/* 语义：r[P2] = P1 */
case OP_Integer: {
  pOut = out2Prerelease(p, pOp);  /* 释放寄存器旧值，返回 aMem[pOp->p2] */
  pOut->u.i = pOp->p1;           /* 写入整数值 */
  break;
}
```

辅助函数 `out2Prerelease` 承担了寄存器的防泄漏检查：它会先判定目标寄存器 `aMem[pOp->p2]` 先前是否持有动态堆内存，按需释放旧资源后，将新的类型标签设置为 `MEM_Int`。在本节开篇的 `EXPLAIN` 示例中，指令 `Integer 30 2 0` 正是将查询条件中的过滤阈值 30 稳妥地存入寄存器 r[2]。

### 6.2 OP_OpenRead：打开 B 树游标

在访问底层持久化数据之前，虚拟机必须先建立一条访问通道：

```c
/* 语义：以只读方式打开根页为 P2 的 B-tree，绑定到游标 P1 */
case OP_OpenRead: {
  int p2 = pOp->p2;                /* B-tree 根页号 */
  int iDb = pOp->p3;              /* 数据库索引（0=main） */
  Btree *pX = db->aDb[iDb].pBt;

  int nField;
  KeyInfo *pKeyInfo = 0;
  if( pOp->p4type == P4_KEYINFO ){
    pKeyInfo = pOp->p4.pKeyInfo;  /* 索引 B-tree：P4 携带键比较信息 */
    nField = pKeyInfo->nAllField;
  } else {
    nField = pOp->p4.i;           /* 表 B-tree：P4 是列数 */
  }

  VdbeCursor *pCur = allocateCursor(p, pOp->p1, nField, CURTYPE_BTREE);
  pCur->pgnoRoot = p2;
  rc = sqlite3BtreeCursor(pX, p2, 0/*只读*/, pKeyInfo, pCur->uc.pCursor);
  pCur->isTable = (pOp->p4type != P4_KEYINFO);
  break;
}
```

这条指令清晰地展现了操作数 `p4` 联合体的多态威力：当打开的是普通数据表时，`p4type` 为整型，`p4.i` 直接记录该表包含的列数；而当打开的是索引 B 树时，游标在检索记录时必须知道各个列的字符编码与比较排序规则，此时 `p4type` 被指定为 `P4_KEYINFO`，`p4.pKeyInfo` 传入的是指向完整比较元数据的指针。

### 6.3 OP_Rewind / OP_Next：驱动表扫描循环

遍历表记录的核心骨架由 `Rewind` 与 `Next` 成对搭档完成：`Rewind` 负责初次定位到第一行，而 `Next` 则驱动游标逐行下移。

```c
/* Rewind：定位到第一行。如果表为空则跳转到 P2 */
case OP_Rewind: {
  VdbeCursor *pC = p->apCsr[pOp->p1];
  int res;
  rc = sqlite3BtreeFirst(pC->uc.pCursor, &res);  /* 定位到第一条记录 */
  pC->cacheStatus = CACHE_STALE;
  if( res ) goto jump_to_p2;   /* 表为空 → 跳过循环体 */
  break;
}

/* Next：前进到下一行。如果还有数据则跳转到 P2（循环回去） */
case OP_Next: {
  VdbeCursor *pC = p->apCsr[pOp->p1];
  rc = sqlite3BtreeNext(pC->uc.pCursor, pOp->p3);
  pC->cacheStatus = CACHE_STALE;
  if( rc == SQLITE_OK ){
    pC->nullRow = 0;
    goto jump_to_p2_and_check_for_interrupt;  /* 还有行 → 跳回循环头 */
  }
  rc = SQLITE_OK;
  pC->nullRow = 1;  /* 没有更多行 → 顺序执行（退出循环） */
  break;
}
```

值得注意的是两者的跳转方向差异：**`Rewind` 向前跳转**（当表为空或游标无效时，越过整个循环体直接跳转到结束地址）；而 **`Next` 向后跳转**（只要底层 B 树还有后续记录，就重新跳回循环体首部执行过滤和列提取）。这与现代编译器将 C 语言 `while` 循环降阶为汇编时的控制流布局完全一致：前置探测入口守卫，循环尾部实施条件回跳。

### 6.4 OP_Column：提取并反序列化列值

一旦游标稳稳落在某一条物理记录上，接下来就需要按需抽取指定字段：

```c
/* 语义：r[P3] = 游标 P1 当前行的第 P2 列 */
case OP_Column: {
  VdbeCursor *pC = p->apCsr[pOp->p1];
  u32 p2 = pOp->p2;          /* 列索引 */
  Mem *pDest = &aMem[pOp->p3]; /* 目标寄存器 */

  /* 如果缓存失效，从 B-tree 获取记录原始数据 */
  aRow = sqlite3BtreePayloadFetch(pCrsr, &pC->szRow);

  /* 解析记录头部：依次读取每一列的 serial type，计算偏移量 */
  /* ... */

  /* 根据 serial type 反序列化列值到目标寄存器 */
  u32 t = aType[p2];        /* 这一列的 serial type */
  if( t < 12 ){
    sqlite3VdbeSerialGet(zData, t, pDest);  /* 整数、浮点或 NULL */
  } else {
    /* 字符串或 BLOB */
    int len = (t - 12) / 2;
    memcpy(pDest->z, zData, len);
    pDest->flags = aFlag[t & 1];  /* MEM_Blob 或 MEM_Str */
  }
  break;
}
```

`OP_Column` 是整个虚拟机中逻辑最为密集的指令之一，完整实现长达两百多行。SQLite 在磁盘页上将整行数据紧凑压缩为一段字节序列：起始处是记录头（Header），依次存放着由变长整数（Varint）编码的列类型编号（Serial Type）；随后才是紧密排列的实际列数据。`OP_Column` 必须先解码记录头，累计前面各列的实际字节跨度，算得目标列的物理起始偏移，最后再将其反序列化并填入寄存器。

为了规避多次抽取同一行不同字段时的二次解析损耗，`VdbeCursor` 细致地维护了 `nHdrParsed` 和列偏移数组 `aOffset`。当查询需要读取多个列时，只有第一次提取会按需推进记录头的解码进度；后续的列提取只要索引落在已解析区间内，便可直接按偏移定位数据，消除了重复解析的二次方开销。

### 6.5 OP_ResultRow：流式返回结果行

当一行数据经过所有条件过滤、各列字段均已填入寄存器后，虚拟机调用 `OP_ResultRow` 向上层返回：

```c
/* 语义：寄存器 r[P1] 到 r[P1+P2-1] 构成一行结果 */
case OP_ResultRow: {
  p->pResultRow = &aMem[pOp->p1];      /* 指向结果行的起始寄存器 */
  p->pc = (int)(pOp - aOp) + 1;        /* 保存 PC（指向下一条指令） */
  rc = SQLITE_ROW;
  goto vdbe_return;                     /* 退出执行循环 */
}
```

与绝大多数执行完毕后自动步进的指令不同，`OP_ResultRow` **绝不执行 `break`**。它把当前结果行的起始寄存器指针记录在 `p->pResultRow`，并将下一条待执行指令的序号暂存在 `p->pc` 中，随即直接跳转至返回点退出整个分发函数。

这正是 `sqlite3_step()` 能够以生成器风格对外呈现流式结果的关键枢纽：调用者拿到 `SQLITE_ROW` 之后，可以通过 `sqlite3_column_*` 族函数安全读取当前行；当上层再次调用 `sqlite3_step()` 时，虚拟机又会精确地从中断点继续推进，仿佛中途从未离开。

### 6.6 OP_Goto 与分支跳转：控制流的汇编级表达

条件判断与无条件跳转构成了控制流调度的基石：

```c
/* 无条件跳转到地址 P2 */
case OP_Goto: {
  goto jump_to_p2_and_check_for_interrupt;
}

/* 如果 r[P1] 为真则跳转到 P2。P3 控制 NULL 的处理 */
case OP_If: {
  int c = sqlite3VdbeBooleanValue(&aMem[pOp->p1], pOp->p3);
  if( c ) goto jump_to_p2;
  break;
}
```

在 SQL 的 `WHERE` 条件中，诸如 `OP_If` 与 `OP_IfNot` 可以直接充当通用布尔分支。但如果细致观察前文示例中的字节码，我们会发现 `WHERE age > 30` 并没有生成先比较、再布尔跳转的两步指令，而是直接编译成了一条 `OP_Le`（小于等于时跳转）。

这种设计融合了比较与条件跳转。指令 `Le 2 7 1` 的语义直接表达为：“如果寄存器 r[1] 的数值小于等于 r[2]，则直接跳转到第 7 行的 `Next` 指令”。在指令级别直接完成对不符合条件行的快速跳过，不仅缩减了编译后的字节码总量，还为虚拟机的分发循环减少了一整轮无谓的指令调度。参数 `p5` 中编码的位标记则进一步锁定了三值逻辑规则（例如当参与比较的操作数为 NULL 时是否触发跳转），确保与 SQL 标准语义精确吻合。

## 7. 实例：一条 SELECT 的完整执行

把上述机制拼装起来，我们可以重新审视本文开头的那段字节码，追踪它在物理执行时的完整动态。

这段指令在布局上巧妙地划分为**初始化段**与**主扫描循环**两部分。细心的读者可能会好奇：为什么程序起始的地址 0 是一条 `Init` 指令，而且要大费周折地跳到地址 9 执行？

```text
0     Init           0     9     0                    0
...
9     Transaction    0     0     1     0              1
10    Integer        30    2     0                    0
11    Goto           0     1     0                    0
```

这实际上是代码生成器为了优化 CPU 指令高速缓存（L1i Cache）而采取的成熟手法。初始化操作（例如开启事务、将常量 30 预装载到寄存器 r[2]）在整个生命周期内只执行一次，如果硬塞在主程序首部，就会割裂高频执行的主循环。将初始化指令移到程序尾部，由地址 0 的 `Init` 启动时跳过去执行，初始化完毕后再由 `Goto` 跳回地址 1，能让地址 1 到 8 的主循环在物理内存中保持绝对的紧凑与连续。

随后，主扫描循环开始有条不紊地推进：
1. **地址 1（OpenRead）**：在数据库连接中打开表 `users` 的 B 树游标 0。
2. **地址 2（Rewind）**：驱动游标 0 定位到数据表的第一条记录。
3. **地址 3（Column）**：从游标当前所指向的行中抽取第 2 列（`age`），存入寄存器 r[1]。
4. **地址 4（Le）**：比较 r[1] 与 r[2]（阈值 30）。若小于等于 30，说明不满足过滤条件，直接向前跳转到地址 7 的 `Next`，绕过后续的列抽取。
5. **地址 5–6（Column & ResultRow）**：若条件满足，抽取第 1 列（`name`）存入 r[3]，随后触发 `ResultRow` 将 r[3] 向上暴露给调用方，并暂停虚拟机。
6. **地址 7–8（Next & Halt）**：当下一次 `step` 被调用时，从地址 7 的 `Next` 恢复，游标推进至下一行并向后跳回地址 3 继续循环；直至记录耗尽，循环退出落入 `Halt` 结束执行。

假设表中存有 `(1, 'Alice', 25)`、`(2, 'Bob', 35)` 和 `(3, 'Charlie', 40)` 三条记录，Alice 在地址 4 处因 `25 <= 30` 触发跳转直接被过滤；Bob 与 Charlie 则顺利抵达地址 6 分别暂停并返回，最终准确产出两行查询结果。

::: generate vdbe-trace-example kind=example concepts=vdbe-bytecode-format,vdbe-mem-struct,vdbe-struct

面向正在学习 C 语言的读者，逐步跟踪 `SELECT name FROM users WHERE age > 30` 编译出的字节码的执行过程。使用上面 EXPLAIN 输出中的实际指令序列：

```
0   Init           0     9     0                    0
1   OpenRead       0     2     0     3              0
2   Rewind         0     8     0                    0
3     Column       0     2     1                    0
4     Le           2     7     1     BINARY-8       84
5     Column       0     1     3                    0
6     ResultRow    3     1     0                    0
7   Next           0     3     0                    1
8   Halt           0     0     0                    0
9   Transaction    0     0     1     0              1
10  Integer        30    2     0                    0
11  Goto           0     1     0                    0
```

假设 users 表（`id INTEGER PRIMARY KEY, name TEXT, age INTEGER`）有三行数据：(1, Alice, 25), (2, Bob, 35), (3, Charlie, 40)，其中 name 是第 1 列，age 是第 2 列。

逐条指令解释执行过程：
1. Init 跳转到地址 9
2. Transaction 开启读事务
3. Integer 把 30 存入 r[2]
4. Goto 跳到地址 1
5. OpenRead 打开 users 表的 B-tree 游标 0
6. Rewind 定位到第一行 (1, Alice, 25)
7. 循环体：Column 读 age 到 r[1]，Le 比较 r[1] 和 r[2]（25 <= 30 为真，跳到地址 7 的 Next，跳过这行）
8. Next 前进到第二行并跳回地址 3：Bob 的 age=35 > 30，不跳转，Column 读 name 到 r[3]，ResultRow 返回 "Bob"
9. 下一次 step 从地址 7 的 Next 继续，跳回地址 3：Charlie 的 age=40 > 30，返回 "Charlie"
10. Next 发现没有更多行，顺序执行到 Halt

用表格或编号列表展示每一步的 PC、指令、关键寄存器变化和游标状态，并指出初始化部分放在程序尾部、由 Init 跳转过去先执行。

:::

## 8. 用 EXPLAIN 观察字节码

在日常开发与内核学习中，我们完全不必依赖侵入式的源码断点来理解虚拟机的运作。SQLite 的交互式终端提供了极其强大的内省工具：

在任何标准 SQL 语句前冠以 `EXPLAIN` 关键字，SQLite 便会直接输出该语句编译所得的完整 VDBE 字节码清单；而在语句前加上 `EXPLAIN QUERY PLAN`，则会展示更高维度的查询规划树（例如是否命中了复合索引、是否触发了临时排序器或全表扫描）。掌握这两项工具，不仅能让你看透 SQL 底层的指令流动，也是后续优化复杂查询与索引设计时不可或缺的利器。

::: generate vdbe-explain-usage kind=explanation concepts=vdbe-bytecode-format

面向正在学习 C 和 SQL 的读者，简要介绍如何在实践中使用 EXPLAIN 和 EXPLAIN QUERY PLAN 来观察 SQLite 的字节码。说明 EXPLAIN 显示完整的指令序列，EXPLAIN QUERY PLAN 显示更高层的查询计划（使用了哪个索引、是全表扫描还是索引查找）。给出 2-3 个实际的 sqlite3 命令行示例，鼓励读者自己尝试。

:::

## 9. 习题

### 9.1 巩固

<Exercise id="50656" :d="2" :w="2">

不运行程序，按 §7 的方法逐条走一遍下面这段字节码（`users` 表的三行是 (1, Alice, 25)、(2, Bob, 35)、(3, Charlie, 40)，`name` 是第 1 列，`age` 是第 2 列）：

```text
addr  opcode         p1    p2    p3    p4
0     Init           0     11    0
1     OpenRead       0     2     0     3
2     Rewind         0     10    0
3       Column       0     2     1
4       Le             2     9     1     BINARY-8
5       Column       0     1     1
6       Eq             3     9     1     BINARY-8
7       Column       0     1     4
8       ResultRow      4     1     0
9     Next           0     3     0
10    Halt           0     0     0
11    Transaction    0     0     1     0
12    Integer        30    2     0
13    String8        0     3     0     Bob
14    Goto           0     1     0
```

1. 逐行列出每次循环里 r[1]、r[2]、r[3]、r[4] 的值与 `pc` 的去向，指出哪几行会被 `ResultRow` 输出。
2. 地址 5 的 `Column 0 1 1` 与地址 3 的 `Column 0 2 1` 写的是同一个寄存器 r[1]。这样复用安全吗？地址 6 的 `Eq` 用的是哪两个寄存器，谁与谁比较？
3. 把地址 13 的字符串改成 `Charlie`，输出变成什么？把地址 4 的 `Le` 的两个输入操作数互换（P1 改 1、P3 改 2），输出又变成什么？后一种改法得到的条件是不是原条件的「反」？往表里加一行 `(4, 'Dana', 30)`，两种写法各输出哪些行？

做完用 `sqlite3` 命令行执行 `SELECT name FROM users WHERE age > 30 AND name <> 'Bob'` 核对第 1 问。验收：第 1 问的寄存器值与实测输出一致；第 2 问指出复用安全的原因是 r[1] 里的 age 在本次循环已经用完；第 3 问指出互换后跳过条件从 `age <= 30` 变成 `30 <= age`，输出只剩 Alice，而 Dana 在两种写法里都跳过——`Le` 的语义是「r[P3] <= r[P1] 时跳转」，互换操作数得到的是另一个方向的比较，不是逻辑取反。

</Exercise>

<Exercise id="50657" :d="3" :w="2">

六个比较运算符编译出来的指令与源码里的运算符并不相同。对同一张 `users` 表分别执行 `EXPLAIN`，填一张表：

| WHERE 子句 | 指令 | p1 | p3 |
|:---|:---|:---|:---|
| `age > 30` | | | |
| `age >= 35` | | | |
| `age < 30` | | | |
| `age <= 30` | | | |
| `age = 35` | | | |
| `age <> 35` | | | |

1. 补全表格，说明为什么 `>` 编译成 `Le` 而不是 `Gt`（提示：比较指令的语义是「若条件成立则跳转」，而跳转的目标是循环尾部的 `Next`）。
2. 30 这个常量由哪条指令写进哪个寄存器？六行里被比较的列值又落在哪个寄存器？说明 `p1`、`p3` 在比较指令里各代表什么。
3. 往表里插一行 `(4, 'Dana', NULL)`，再执行 `SELECT name FROM users WHERE age > 30`。Dana 不会出现，解释原因，并指出 `Le` 指令的 p5（十进制 84）里哪一部分让 NULL 直接跳转。

验收：六行的指令分别为 `Le`、`Lt`、`Ge`、`Gt`、`Ne`、`Eq`；第 3 问指出 `age > 30` 对 NULL 求值为 NULL，`Le` 的 p5 带 `SQLITE_JUMPIFNULL` 位，比较结果为 NULL 时按跳转处理。

</Exercise>

<Exercise id="50658" :d="3" :w="3">

`sqlite3_stmt_status(stmt, SQLITE_STMTSTATUS_VM_STEP, reset)` 返回这条语句执行过的 VDBE 指令数。用它观察 §7 那条查询：

1. 每次 `sqlite3_step` 之后读取一次 `VM_STEP`（第三个参数传 0，表示不重置），记录三次调用的增量。预测值分别是多少，再实测。
2. 用 `sqlite3_progress_handler(db, 1, cb, NULL)` 安装一个每次 VDBE 执行一条指令就调用一次的回调，统计每次 `sqlite3_step` 里的回调次数，与第 1 问的增量对比。
3. 把查询改成 `SELECT name FROM users`（没有 WHERE），再测一遍。为什么第一条 `step` 的增量变小了？
4. 把回调的返回值改成「第 6 次以后返回非 0」，记录 `sqlite3_step` 的返回码、回调次数、`VM_STEP` 与 `sqlite3_errmsg`。

验收：三次增量为 13、5、2（合计 20，与 `EXPLAIN` 的指令数和行数对得上）；回调次数与增量逐次相等；没有 WHERE 的那条第一条增量是 7；第 4 问在 `SQLITE_INTERRUPT`（9）处停下，回调 6 次、`VM_STEP` 为 9，错误信息是 `interrupted`。说明 `VM_STEP` 统计的是**执行过**的指令，不是字节码的总条数。

</Exercise>

### 9.2 编程

<Exercise id="50659" :d="4" :w="3">

实现一个 6 条指令的迷你虚拟机，体会 §5 的主循环。指令与语义自己定，但必须包含加载常量、加法、比较跳转、无条件跳转、输出一行、停机六种。把它跑通下面这段程序，输出应当是 `55`：

```text
0   Integer    r1 = 0
1   Integer    r2 = 1
2   Integer    r3 = 10
3   Integer    r4 = 1
4   Le         if r2 <= r3 goto 6
5   Goto       9
6   Add        r1 = r1 + r2
7   Add        r2 = r2 + r4
8   Goto       4
9   ResultRow  output r1
10  Halt
```

要求：

1. 指令存成结构体数组，`pc` 是数组下标；主循环用 `for` 加 `switch`，跳转指令直接改 `pc`。不要用函数指针表。
2. `ResultRow` 输出当前寄存器并暂停：`run()` 返回 1，再次调用从下一条继续；`Halt` 返回 0。写一个 `while (run(&vm))` 的主程序。
3. 统计执行过的指令条数，与手工数出来的一致。
4. 增加 `Multiply`，把程序改成计算 1 到 10 的平方和，期望 385；输出 385 之后统计的步数写进报告。

验收：输出恰好一行 `55`（第 4 问是 `385`），步数与你手工推算一致；`-Wall -Wextra` 无警告。说明这段程序里哪条指令对应 SQLite 的 `OP_ResultRow` 暂停、哪条对应 `OP_Next` 的回跳。

</Exercise>

<Exercise id="50660" :d="4" :w="3">

`Mem` 的 `flags` 与 `zMalloc`/`xDel` 决定了寄存器里的值由谁释放。把 `sqlite3.c` 直接 `#include` 进自己的程序（不要链接它），然后：

1. 对 `SELECT name FROM users WHERE age > 30` 在每次 `step` 之后打印 r[1]、r[2]、r[3] 的 `flags`、`z`、`zMalloc`、`xDel`，用 §2 的宏解码 flag 的每一位。
2. 对 `SELECT ?1, ?2` 分别用 `SQLITE_STATIC` 和 `SQLITE_TRANSIENT` 绑定同一块栈缓冲区，`step` 之后打印两个寄存器的 `z` 与 `zMalloc`，比较它们与源缓冲区的地址关系。然后把源缓冲区的内容改掉，再读回结果。
3. 打印 `(VdbeCursor *)p->apCsr[0]` 的 `nField`、`nHdrParsed`、`aType[0..nField-1]`，分别对 `SELECT name FROM users` 与 `SELECT * FROM users` 各跑一次，解释 `nField` 为什么不同。
4. 回答：`MEM_Static` 与 `MEM_Dyn` 的区别在寄存器被覆盖时体现为哪一句代码？`MEM_Term` 位对应 `Mem.n` 与 `z` 的什么关系？

验收：第 2 问 `SQLITE_STATIC` 的寄存器 `z` 与源缓冲区地址相同、改内容后读到新值，`SQLITE_TRANSIENT` 的地址不同、读到旧值；第 3 问一个 `nField=2`、一个 `nField=3`，`aType` 分别是 `0 23` 与 `0 23 1`（数值随表结构与列数而定）。这些字段是内部结构，升级合并源码后布局可能变化，报告里写明你用的版本。

</Exercise>

<Exercise id="50661" :d="5" :w="4">

写一个字节码静态分析器 `vdbedump.c`：输入一条 SQL，用 `EXPLAIN` 取出指令表，输出按基本块划分的清单。

1. 把每条跳转指令（`Init`、`Goto`、`Rewind`、`Next`、`Le`、`If`、`IfNot`、`Seek*` 等，遇到不认识的按「p2 是跳转目标」处理）的目标地址记下来，标出哪些地址是跳转入口。
2. 按「上一条是跳转或本条是跳转目标」切分基本块，每块打印起止地址与块内指令。
3. 找出所有向后跳转（目标地址小于当前地址），它们构成循环。打印循环头、循环尾与循环体包含的块。
4. 对 `SELECT name FROM users WHERE age > 30` 与 `SELECT name FROM users ORDER BY name` 各跑一次，比较循环个数与块的个数。后者为什么多出几块？

验收：第一条查询识别出一个以 `Next` 结尾的循环，循环头是地址 3；第二条查询多出排序器相关的块，且 `SorterNext` 也构成一个循环。分析器完全不执行 SQL，只看 `EXPLAIN` 的行。

</Exercise>

### 9.3 综合与挑战

<Exercise id="50662" :d="5" :w="4">

在 50659 的迷你虚拟机上做一次常量折叠，观察代码生成、跳转与执行三者的关系。

1. 用 `EXPLAIN SELECT 1+2*3` 拿到真实的字节码，把它抄成你自己的指令数组：`Init` 跳到常量区，常量区依次 `Integer 1`、`Integer 2`、`Integer 3`、`Multiply`、`Add`，最后 `Goto` 回主程序。
2. 写一个 `fold()`：扫描连续的「两个 `Integer` 加一条二元运算」，把结果算出来替换成一条 `Integer`，并让后续指令不再依赖被删除的寄存器。
3. 折叠之后指令数组里出现空洞。实现一步「压实」：把有效指令往前挪，同时重写所有跳转目标（注意 `Init` 的 p2 与 `Goto` 的 p2 都要减）。
4. 在折叠前后各跑一次执行器，结果必须都是 7；打印折叠前后 `nOp` 与指令清单。
5. 回答：为什么不改跳转目标就等于改错了程序？在真实 SQLite 里，`vdbe_addoptrace` 打印的负数跳转地址（见 6.4）说明代码生成器用的是哪一种回填策略？

验收：折叠后指令数减少、结果不变；压实后所有跳转仍落在正确的指令上；报告里贴出两份指令清单和 `nOp` 的前后值。

</Exercise>

<Exercise id="50663" :d="6" :w="5" scope="*">

用 `VM_STEP` 与 `EXPLAIN QUERY PLAN` 量化一次查询的执行代价，回答「索引一定更省吗」。

1. 建一张 1000 行的 `users(id INTEGER PRIMARY KEY, name TEXT, age INTEGER)`，`age` 在 20–519 之间循环取值。
2. 对 `SELECT name FROM users WHERE age > 300` 在无索引、有 `age` 索引两种情况下分别记录：结果行数、`VM_STEP`、`FULLSCAN_STEP`、`EXPLAIN QUERY PLAN` 的一行输出。再把查询改成只取 `age`（覆盖索引），在同样两种情况下各测一次，比较它比取 `name` 少了哪一步。
3. 再测 `SELECT count(*) FROM users`、`SELECT name FROM users ORDER BY name`、`SELECT name FROM users ORDER BY id`，记录 `VM_STEP` 与 `SORT` 计数。`ORDER BY id` 为什么不排序？
4. 把第 2 问的阈值改成 `age > 20`（几乎所有行都命中），重新测三种情况。解释为什么此时索引的优势变小甚至消失。
5. 结合 §2 的 `Mem`、§6.5 的 `ResultRow`，写一段说明：`VM_STEP` 把「读一列值」和「比较一次」都算作一步，它衡量的是什么，不能衡量什么（提示：一次性读整页与逐页读取的差别）。

验收：无索引时 `FULLSCAN_STEP` 约为 999，有索引时为 0；索引版本取 `name` 时 `VM_STEP` 约 1760、只取 `age` 时约 1321，后者少的是对表页的 `Column`（覆盖索引直接从索引页取列值）；`ORDER BY id` 的 `SORT` 为 0，`ORDER BY name` 为 1；第 4 问给出两组实测数字（本例为 998 行命中时索引约 4000 步、全表扫描约 5003 步）并解释。`count(*)` 走的是 `Count` 指令而不是逐行循环，所以只需 9 步左右。

</Exercise>
