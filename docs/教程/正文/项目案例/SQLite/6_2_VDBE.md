# 6.2 源码阅读：SQLite 虚拟机（VDBE）

SQLite 不直接解释 SQL，而是先把 SQL 编译成字节码，再由一台虚拟机逐条执行。这台虚拟机叫 VDBE（Virtual Database Engine），是 SQLite 最核心的运行时组件。用 `EXPLAIN` 可以直接看到编译出的字节码（表结构为 `CREATE TABLE users(id INTEGER PRIMARY KEY, name TEXT, age INTEGER)`）：

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

Python、Lua 也采用同样的方式：源码先编译为字节码，再由虚拟机解释执行。区别在于 SQLite 的指令集专门为数据库操作设计，有打开 B-tree 游标、提取列值、返回结果行等专用指令。

在[架构概述](./6_1_架构概述.md)中，VDBE 位于代码生成器（前端）和 B-tree/Pager（后端）之间。前端把 SQL 翻译成字节码交给 VDBE，VDBE 执行字节码时通过 B-tree API 读写数据。本文介绍 VDBE 本身的内部实现。

::: generate vdbe-why-vm kind=explanation concepts=vdbe-bytecode-format

面向正在学习 C 语言的读者，解释为什么 SQLite 选择"先编译后解释"的架构而不是直接解释 SQL。要点：(1) SQL 是声明式语言，不能直接执行；(2) 字节码提供了稳定的内部表示，把复杂的 SQL 优化（查询计划选择、索引使用决策）集中在编译阶段，执行阶段只需简单的指令分发；(3) 同一条预编译语句可以绑定不同参数反复执行，避免重复编译。类比 Python 的 .pyc 文件或 Java 的 .class 文件帮助读者理解。

:::

## 1. 字节码格式：VdbeOp 结构体

::: concept vdbe-bytecode-format title="字节码指令的格式"

每条字节码指令是一个 `VdbeOp` 结构体（别名 `Op`）。六个字段完整描述一条指令需要做什么：

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

`p1`--`p3` 是三个整数操作数，含义随 opcode 不同而变化。`p4` 是一个联合体，可以携带指针、字符串等复杂数据。`p5` 通常作为标志位使用。

:::

所有指令格式固定，各操作数的含义随操作码变化。x86 指令是变长的，解码时要先根据操作码确定指令长度；VDBE 的每条指令大小相同，按数组下标即可取到下一条指令。SQLite 目前定义了约 190 种操作码（见 `opcodes.h`），涵盖算术、比较、跳转、游标操作、聚合、事务控制等。

## 2. 寄存器：Mem 结构体

::: concept vdbe-mem-struct title="寄存器是带类型标签的联合体"

VDBE 是**基于寄存器**的虚拟机。每个寄存器是一个 `Mem` 结构体（也叫 `sqlite3_value`），它是一个带类型标签的联合体，可以持有 SQL 的五种值类型中的任意一种：

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

类型标签 `flags` 决定如何解读联合体：

```c
#define MEM_Null      0x0001   /* NULL */
#define MEM_Str       0x0002   /* 字符串 */
#define MEM_Int       0x0004   /* 整数 */
#define MEM_Real      0x0008   /* 浮点数 */
#define MEM_Blob      0x0010   /* 二进制大对象 */
```

:::

`Mem` 结构体是 C 语言中常见的**带标签的联合体**（tagged union）：联合体 `u` 让整数和浮点数共享同一块内存，`flags` 字段告诉运行时应读取哪个成员。字符串和 BLOB 这类变长数据不放在联合体内，而是由额外的 `z` 指针指向外部缓冲区。

`flags` 的低 6 位（`MEM_AffMask = 0x003f`）表示值类型，高位表示 `z` 的存储方式：

```c
#define MEM_Dyn       0x1000   /* z 指向动态分配的内存，需要调用 xDel 释放 */
#define MEM_Static    0x2000   /* z 指向静态字符串（不需要释放） */
#define MEM_Ephem     0x4000   /* z 指向临时数据（可能随时失效） */
```

这三个标志决定了 `z` 所指内存的归属。寄存器被覆盖时，如果旧值的 `flags` 包含 `MEM_Dyn`，就调用 `xDel(z)` 释放旧缓冲区；`MEM_Static` 和 `MEM_Ephem` 表示寄存器并不拥有这块内存，不需要释放。

::: generate vdbe-register-vs-stack kind=comparison concepts=vdbe-mem-struct

面向正在学习 C 语言的读者，对比基于寄存器的虚拟机和基于栈的虚拟机。要点：(1) 栈式 VM（如 JVM、CPython）用 push/pop 传递操作数，指令短但数量多；(2) 寄存器式 VM（如 Lua 5、VDBE）在指令中直接编码寄存器号，指令长但数量少；(3) VDBE 选择寄存器式的原因：数据库操作经常需要同时持有多个列值、比较结果和游标状态，用寄存器可以避免大量冗余的 push/pop。用一个简单的加法 `a + b` 的例子对比两种方式的指令序列。

:::

## 3. 游标：VdbeCursor 结构体

字节码通过**游标**访问 B-tree 中的数据。每个游标是一个 `VdbeCursor` 结构体，代表一个"打开的表或索引"：

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

`aType` 声明为柔性数组成员（flexible array member），它的实际大小在运行时由 `allocateCursor` 根据表的列数决定。柔性数组成员是 C99 引入的语法，避免了额外的指针间接访问和独立的内存分配。

## 4. 编译后的程序：Vdbe 结构体

前面三个部分组合起来：指令数组（`Op[]`）是程序代码，寄存器数组（`Mem[]`）是数据空间，游标数组（`VdbeCursor*[]`）是 I/O 通道。`Vdbe` 结构体统一管理它们：

::: concept vdbe-struct title="Vdbe 结构体是编译后的完整程序"

`Vdbe` 结构体把执行一条 SQL 语句所需的一切打包在一起：

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

三个数组构成执行的全部状态：

| 数组 | 类型 | 作用 |
|:---|:---|:---|
| `aOp[0..nOp-1]` | `Op` | 字节码指令序列（只读） |
| `aMem[0..nMem-1]` | `Mem` | 寄存器文件（读写） |
| `apCsr[0..nCursor-1]` | `VdbeCursor*` | B-tree 游标（读写） |

:::

从外部 API 看，`sqlite3_prepare_v2()` 创建一个 `Vdbe`（以 `sqlite3_stmt*` 的形式返回给用户），`sqlite3_step()` 调用 `sqlite3VdbeExec()` 执行它，`sqlite3_finalize()` 销毁它。在 prepare 和 finalize 之间，`Vdbe` 可以被反复 step：每次执行到 `OP_ResultRow` 就暂停并返回一行结果，下次 step 从暂停处继续。

`eVdbeState` 字段跟踪这个生命周期：

```c
#define VDBE_INIT_STATE     0   /* 正在构建中（prepare 阶段） */
#define VDBE_READY_STATE    1   /* 准备就绪，尚未开始执行 */
#define VDBE_RUN_STATE      2   /* 正在执行中 */
#define VDBE_HALT_STATE     3   /* 已结束，需要 reset 或 finalize */
```

这是一个状态机。`sqlite3_step()` 只在 READY 或 RUN 状态下才会调用 `sqlite3VdbeExec()`；HALT 状态下会先自动调用 `sqlite3_reset()` 回到 READY，再重新执行。`sqlite3_reset()` 把状态从 HALT 重置为 READY，让同一个编译后的程序可以重新执行。

## 5. 执行引擎：sqlite3VdbeExec

`sqlite3VdbeExec` 是 VDBE 的执行核心，一个将近 9000 行的函数，用一个巨大的 `switch` 分发 180 多种操作码。整个函数是一个 `for` 循环套一个 `switch`，没有函数指针表，也没有间接调用。

::: concept vdbe-dispatch-loop title="指令分发循环的结构"

VDBE 的主循环是最直接的解释器模式：用 `for` 循环遍历指令数组，用 `switch` 根据操作码跳转到对应的处理代码。`pOp` 指针在每次循环迭代时自动递增，跳转指令通过直接修改 `pOp` 来改变控制流。

:::

### 5.1 主循环结构

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

主循环有以下几个设计细节。

**分发方式**：所有 case 集中在一个函数里，牺牲了代码的模块化，换来最直接的分发：编译器可以为 `switch` 生成跳转表，每条指令的分发开销只有一次间接跳转。

**PC 推进方式**：`pOp++` 在 `for` 循环头部完成，每条指令执行完 `break` 后自动前进到下一条，不需要显式的 `pc++`。

**跳转机制**：需要跳转的指令设置 `pOp = &aOp[target - 1]`（减 1 是因为循环头部会加 1），然后 `break`。为了减少重复代码，源码定义了两个共享的跳转标签：

```c
jump_to_p2:
  pOp = &aOp[pOp->p2 - 1];
  break;

jump_to_p2_and_check_for_interrupt:
  pOp = &aOp[pOp->p2 - 1];
  /* 检查 sqlite3_interrupt() 和进度回调 */
  break;
```

**暂停与恢复**：`OP_ResultRow` 把下一条指令的地址保存到 `p->pc`，然后 `goto vdbe_return` 退出循环，返回 `SQLITE_ROW`。下一次调用 `sqlite3VdbeExec` 时，`for` 循环从保存的 `p->pc` 处恢复执行。

### 5.2 局部对象缓存

函数开头把 `p->aOp` 和 `p->aMem` 复制到了局部对象 `aOp` 和 `aMem`。在一个要迭代数百万次的循环中，访问局部对象比经由 `p->aMem[i]` 多一次解引用更快，因为编译器可以把局部对象放进寄存器（这里指 CPU 的寄存器，不是 VDBE 的寄存器）。

## 6. 代表性指令拆解

下面分六组拆解几条有代表性的指令。

### 6.1 OP_Integer：加载常量

这是最简单的指令，把一个 32 位整数常量写入寄存器：

```c
/* 语义：r[P2] = P1 */
case OP_Integer: {
  pOut = out2Prerelease(p, pOp);  /* 释放寄存器旧值，返回 aMem[pOp->p2] */
  pOut->u.i = pOp->p1;           /* 写入整数值 */
  break;
}
```

`out2Prerelease` 是一个辅助函数，它清除目标寄存器的旧内容并设置 `flags = MEM_Int`。开头 `EXPLAIN` 输出中的 `Integer 30 2 0` 就是把常量 30 存入寄存器 r[2]。

### 6.2 OP_OpenRead：打开 B-tree 游标

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

这条指令展示了 `p4` 联合体的实际用途：根据 `p4type` 的不同，`p4` 可以是一个 `KeyInfo*`（索引游标需要知道怎么比较键），也可以是一个 `int`（表游标只需要知道列数）。

### 6.3 OP_Rewind / OP_Next：遍历表行

这两条指令配合实现了对表的循环遍历——Rewind 定位到第一行，Next 推进到下一行：

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

跳转方向并不对称：**Rewind 向前跳**（跳过整个循环），**Next 向后跳**（回到循环头）。这与高级语言 `for` 循环的常见编译方式一致：先检查是否为空，不为空则进入循环体，循环尾部用条件跳转回到循环头。

### 6.4 OP_Column：提取列值

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

`OP_Column` 是最复杂的指令之一，完整实现超过 200 行。SQLite 的记录格式（record format）把一行数据序列化为紧凑的字节流：开头是记录头，依次记录每一列的类型和大小（serial type），后面紧跟各列的实际数据。`OP_Column` 需要解析记录头，找到目标列的偏移量，再反序列化出值。

为了避免每次都从头解析，`VdbeCursor` 缓存了已解析的头部信息（`nHdrParsed`、`aType`、`aOffset`）。如果请求的列已经解析过（`p2 < nHdrParsed`），就直接从缓存读取偏移量和类型，不再解析头部。`SELECT *` 这类读取多列的查询因此只需把记录头解析一遍。

### 6.5 OP_ResultRow：返回结果行

```c
/* 语义：寄存器 r[P1] 到 r[P1+P2-1] 构成一行结果 */
case OP_ResultRow: {
  p->pResultRow = &aMem[pOp->p1];      /* 指向结果行的起始寄存器 */
  p->pc = (int)(pOp - aOp) + 1;        /* 保存 PC（指向下一条指令） */
  rc = SQLITE_ROW;
  goto vdbe_return;                     /* 退出执行循环 */
}
```

这条指令**不执行 break**，而是直接跳出整个 `for` 循环。`sqlite3VdbeExec` 返回 `SQLITE_ROW`，控制权回到 `sqlite3_step()`，用户就可以用 `sqlite3_column_text()` 等函数读取结果。用户再次调用 `sqlite3_step()` 时，执行从保存的 `p->pc`（ResultRow 的下一条指令）处恢复。

### 6.6 OP_Goto / OP_If：控制流

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

`WHERE` 子句中的条件可以编译为 `OP_If` / `OP_IfNot` 指令。条件为假时跳转到循环尾部的 `Next`，跳过当前行；条件为真时顺序执行到输出结果的指令。`p3` 参数处理 SQL 的三值逻辑：寄存器值为 NULL 时，由 `p3` 决定把 NULL 当作真还是假。

对于 `WHERE age > 30` 这样的比较，实际生成的字节码通常不使用 `OP_If`，而是使用比较指令（如 `OP_Le`、`OP_Gt`）。这些指令把比较和条件跳转合并为一步，省去了先比较、再判断的两步开销。开头 EXPLAIN 输出中的 `Le 2 7 1` 表示“如果 r[1] <= r[2] 则跳到地址 7”，即“如果 age <= 30 则跳过这一行”。

## 7. 实例：一条 SELECT 的完整执行

开头的字节码分为初始化段与主循环两部分。地址 0 的 `Init` 先跳到地址 9：`Transaction` 开启读事务，`Integer` 把常量 30 写入 r[2]，`Goto` 跳回地址 1。地址 1–8 是主循环：打开表游标，通过 `Rewind` 和 `Next` 遍历记录，以 `Column` 读取年龄和姓名，由 `Le` 跳过不满足条件的行，由 `ResultRow` 返回满足条件的行。若表中有 (1, 'Alice', 25)、(2, 'Bob', 35)、(3, 'Charlie', 40) 三行，查询依次返回 Bob 和 Charlie。

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

在 sqlite3 命令行工具中，任何 SQL 语句前加上 `EXPLAIN` 即可列出它编译出的字节码；加上 `EXPLAIN QUERY PLAN` 则显示更高层的查询计划。

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
