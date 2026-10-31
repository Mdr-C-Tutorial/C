# 1. 抽象数据类型 (Abstract Data Type, ADT)

## 1. 什么是 ADT？

ADT 就像是一个黑盒子 - 我们只关心它能做什么，不关心它内部如何实现。

## 2. 生活中的例子

- 电视机：我们只需要知道怎么换台、调音量，不需要懂内部电路
- 汽车：我们只需要会驾驶，不需要懂发动机原理

## 3. 编程中的 ADT

ADT = 数据 + 操作

### 3.1 常见 ADT 示例

1. 栈 (Stack):
   - 操作：push(压入)、pop(弹出)
   - 比喻：像叠盘子，后进先出

2. 队列 (Queue):
   - 操作：enqueue(入队)、dequeue(出队)
   - 比喻：像排队买票，先进先出

## 4. 为什么用 ADT？

1. 隐藏实现细节
2. 提高代码可读性
3. 方便修改内部实现

## 5. C 语言实现示例

```c
// 栈的 ADT 定义
typedef struct {
    int data[100];
    int top;
} Stack;

void push(Stack *s, int value) {
    s->data[s->top++] = value;
}

int pop(Stack *s) {
    return s->data[--s->top];
}
```

这段代码只演示接口的形态，省略了初始化和错误检查。

## 6. 抽象层与表示层

上面的栈示例已经体现了 ADT 最重要的思想：调用方只依赖“可以做什么”，而不依赖“内部怎样做”。这一点在 C 语言里尤其重要：C 允许调用方直接访问结构体成员，如果头文件公开了结构体定义，调用方代码就可能依赖其内部布局，以后一旦替换实现，要改的地方会非常多。更好的做法是把数据表示放进 `.c` 文件，`.h` 文件只公开接口，让外部只能通过函数访问对象的状态。

## 7. 接口契约应该写清什么

一个可复用的 ADT 接口至少应说明四项内容：对象的生命周期，即谁负责创建、谁负责销毁；操作的前置条件，例如空栈不可弹出；失败时的行为，例如返回什么状态码；复杂度，例如 `push/pop` 是否保持均摊 O(1)。这几项写清楚以后，调用方不需要了解内部细节就能使用接口；更换实现时，只要新实现满足同样的约定，调用方代码就不必修改。例如，栈的底层可以从静态数组换成动态数组或链表。

## 8. 一个更完整的 C 语言栈 ADT 示例

下面给出一组可以直接拆分为 `stack.h` 与 `stack.c` 的示例。这里采用“状态码 + 输出参数”的接口风格，目的是把“调用失败”和“弹出的值”区分开，避免歧义。

```c
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    STACK_OK = 0,
    STACK_ERR_EMPTY = 1,
    STACK_ERR_OOM = 2
} stack_status;

typedef struct {
    int *data;
    size_t len;
    size_t cap;
} stack;

static void stack_init(stack *s) {
    s->data = NULL;
    s->len = 0;
    s->cap = 0;
}

static void stack_destroy(stack *s) {
    free(s->data);
    s->data = NULL;
    s->len = 0;
    s->cap = 0;
}

static stack_status stack_push(stack *s, int x) {
    if (s->len == s->cap) {
        size_t next = (s->cap == 0) ? 4 : s->cap * 2;
        int *mem = (int *)realloc(s->data, next * sizeof(int));
        if (mem == NULL) {
            return STACK_ERR_OOM;
        }
        s->data = mem;
        s->cap = next;
    }
    s->data[s->len++] = x;
    return STACK_OK;
}

static stack_status stack_pop(stack *s, int *out) {
    if (s->len == 0) {
        return STACK_ERR_EMPTY;
    }
    *out = s->data[--s->len];
    return STACK_OK;
}

int main(void) {
    stack s;
    stack_init(&s);

    stack_push(&s, 10);
    stack_push(&s, 20);
    stack_push(&s, 30);

    int x = 0;
    while (stack_pop(&s, &x) == STACK_OK) {
        printf("%d\n", x);
    }

    stack_destroy(&s);
    return 0;
}
```

运行结果：

::: terminal

30
20
10

:::

## 9. 习题

<Exercise id="31101" :d="4" :w="4">

使用不透明类型实现整数栈 ADT。公开头文件只能包含以下接口，不得暴露栈的表示：

```c
#include <stddef.h>

typedef struct IntStack IntStack;

typedef enum {
    INT_STACK_OK,
    INT_STACK_EMPTY,
    INT_STACK_INVALID,
    INT_STACK_OUT_OF_MEMORY
} IntStackStatus;

IntStack *int_stack_create(void);
void int_stack_destroy(IntStack *stack);
IntStackStatus int_stack_push(IntStack *stack, int value);
IntStackStatus int_stack_pop(IntStack *stack, int *out);
IntStackStatus int_stack_peek(const IntStack *stack, int *out);
size_t int_stack_size(const IntStack *stack);
```

要求：

1. 栈采用动态数组表示，但结构体定义只能出现在实现文件中。
2. `int_stack_destroy(NULL)` 应当安全，`int_stack_size(NULL)` 返回 0。
3. `push` 成功后新元素位于栈顶；分配失败或容量计算溢出时返回 `INT_STACK_OUT_OF_MEMORY`，并保持原栈不变。
4. `pop` 和 `peek` 的参数无效时返回 `INT_STACK_INVALID`，栈为空时返回 `INT_STACK_EMPTY`；两种失败都不得修改 `*out`。
5. `peek` 不删除元素，`pop` 删除并返回栈顶元素。
6. `push` 的摊还时间复杂度为 $O(1)$，`pop`、`peek` 和 `size` 的时间复杂度为 $O(1)$。

</Exercise>

<Exercise id="31102" :d="6" :w="5" scope="**">

实现采用开放寻址和线性探测的字符串集合。公开接口不得暴露槽数组：

```c
#include <stdbool.h>
#include <stddef.h>

typedef struct StringSet StringSet;

StringSet *string_set_create(void);
void string_set_destroy(StringSet *set);
bool string_set_insert(
    StringSet *set,
    const char *key,
    bool *inserted
);
bool string_set_contains(
    const StringSet *set,
    const char *key
);
size_t string_set_size(const StringSet *set);
```

要求：

1. 集合复制并拥有成功插入的字符串；调用方仍拥有传入的字符串。
2. 重复插入应成功返回并设置 `*inserted = false`，集合大小不变。
3. 槽数组容量始终为 2 的幂；下一次插入会使装载因子超过 $3/4$ 时，先扩容到两倍并重新散列已有字符串。
4. 不得用散列值相等代替字符串相等。
5. 参数无效、容量计算溢出或分配失败时返回 `false`，保持集合中的键不变，并且不得修改 `*inserted`。
6. `string_set_destroy(NULL)` 应当安全；销毁集合时释放它拥有的全部字符串。
7. 在散列分布合理的前提下，查询和插入的期望时间复杂度为 $O(1)$。

</Exercise>

<Exercise id="31103" :d="6" :w="5" scope="**">

为同一个整数序列 ADT 编写动态数组和双向链表两种可替换实现。公开头文件固定为：

```c
#include <stddef.h>

typedef struct IntSequence IntSequence;

typedef enum {
    INT_SEQUENCE_OK,
    INT_SEQUENCE_INVALID,
    INT_SEQUENCE_RANGE,
    INT_SEQUENCE_OUT_OF_MEMORY
} IntSequenceResult;

IntSequence *int_sequence_create(void);
void int_sequence_destroy(IntSequence *sequence);
IntSequenceResult int_sequence_insert(
    IntSequence *sequence,
    size_t index,
    int value
);
IntSequenceResult int_sequence_erase(
    IntSequence *sequence,
    size_t index,
    int *out_value
);
IntSequenceResult int_sequence_get(
    const IntSequence *sequence,
    size_t index,
    int *out_value
);
size_t int_sequence_size(const IntSequence *sequence);
```

要求：

1. 分别创建 `int_sequence_array.c` 和 `int_sequence_linked.c`；每次构建只链接其中一个实现文件，同一份调用方代码无需修改。
2. 两个实现文件都不得在公开头文件中暴露表示，也不得增加调用方必须使用的公开函数。
3. `insert` 允许 `index == size`；`erase` 和 `get` 只允许 `index < size`。
4. 参数无效时返回 `INT_SEQUENCE_INVALID`，下标越界时返回 `INT_SEQUENCE_RANGE`；失败时不得修改序列和 `*out_value`。
5. 动态数组实现在扩容失败或容量计算溢出时返回 `INT_SEQUENCE_OUT_OF_MEMORY`，并保持原序列不变；尾部插入的摊还时间复杂度为 $O(1)$。
6. 双向链表实现同时保存首尾指针，从距离目标下标较近的一端开始查找；首尾插入和删除的时间复杂度为 $O(1)$。
7. `int_sequence_destroy(NULL)` 应当安全，`int_sequence_size(NULL)` 返回 0。
8. 两种实现都必须释放自身取得的全部资源。

</Exercise>

<Exercise id="31104" :d="1" :w="1">

设计并实现不可变有理数抽象数据类型（Rational ADT），支持值语义的代数运算与前置条件/代数不变式契约约束：

```c
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int64_t num;
    int64_t den;
} Rational;

bool rational_create(int64_t num, int64_t den, Rational *out);
bool rational_add(Rational a, Rational b, Rational *out);
bool rational_sub(Rational a, Rational b, Rational *out);
bool rational_mul(Rational a, Rational b, Rational *out);
int rational_compare(Rational a, Rational b);
```

要求：

1. **代数不变式**：分母严格大于 0（$den > 0$）；分子与分母互质（$\gcd(|num|, den) == 1$）；数值 0 的唯一规范表示为 $0/1$。
2. **构造契约**：分母为 0 或输出指针为 `NULL` 时返回 `false`，不得修改 `*out`；若分母为负，须将符号转移至分子。
3. **溢出安全性**：加、减、乘运算若发生 64 位有符号整数溢出，必须返回 `false`，不得引发未定义行为，且不得修改输出对象。
4. **全序比较**：`rational_compare` 比较两个有理数的大小：若 $a < b$ 返回负数，若 $a == b$ 返回 0，若 $a > b$ 返回正数；比较过程严防算术溢出。
5. **复杂度**：除辗转相除法开销外，所有运算均为 $O(1)$ 时间复杂度。

</Exercise>

<Exercise id="31105" :d="2" :w="2">

实现零动态内存分配的定长滑动窗口打点限流器抽象数据类型（RateLimiter ADT），内部以环形缓冲维护单调递增的时间戳序列：

```c
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct RateLimiter RateLimiter;

struct RateLimiter {
    uint64_t window_ms;
    size_t max_hits;
    uint64_t *timestamps;
    size_t capacity;
    size_t head;
    size_t count;
};

void rate_limiter_init(
    RateLimiter *limiter,
    uint64_t window_ms,
    size_t max_hits,
    uint64_t *timestamp_buffer,
    size_t buffer_capacity
);
bool rate_limiter_acquire(RateLimiter *limiter, uint64_t now_ms);
size_t rate_limiter_current_load(RateLimiter *limiter, uint64_t now_ms);
```

要求：

1. **外部存储提供契约**：ADT 内部不调用 `malloc`，所需环形缓冲内存由外部调用方显式注入；初始化后调用方仅通过公开函数访问，不得直接读写结构体成员。
2. **单调性前置条件**：调用方传入的当前时间戳 `now_ms` 必须关于时间单调非递减；若检测到时间倒退，`acquire` 返回 `false`，保持内部状态不变。
3. **过期淘汰机制**：打点请求发生或查询负载时，自动淘汰掉严格落在当前滑动窗口闭区间 $[now\_ms - window\_ms + 1, now\_ms]$ 之外的所有过期时间戳。
4. **配额判定**：淘汰过期记录后，若有效打点数未达到 `max_hits` 且未超出物理缓冲容量，记录本次时间戳并返回 `true`；否则拒绝请求返回 `false`。
5. **复杂度**：利用环形数组队列，每次淘汰与打点的均摊时间复杂度为 $O(1)$，空间复杂度严格限制在预分配的缓冲区大小。

</Exercise>

<Exercise id="31106" :d="4" :w="4">

实现支持在 $O(1)$ 时间复杂度内获取当前最小元素的整数最小栈抽象数据类型（MinStack ADT）：

```c
#include <stdbool.h>
#include <stddef.h>

typedef struct MinStack MinStack;

MinStack *min_stack_create(void);
void min_stack_destroy(MinStack *stack);
bool min_stack_push(MinStack *stack, int value);
bool min_stack_pop(MinStack *stack, int *out_value);
bool min_stack_top(const MinStack *stack, int *out_value);
bool min_stack_get_min(const MinStack *stack, int *out_min);
size_t min_stack_size(const MinStack *stack);
```

要求：

1. **不透明抽象封装**：实现细节对调用方完全不可见，调用方无法获知内部是采用辅助同步栈还是单数组复合条目。
2. **常数级查询契约**：`min_stack_get_min` 必须在严格的 $O(1)$ 时间复杂度内返回当前栈内已有元素的最小值，严禁全栈遍历。
3. **强异常安全保证**：`push` 扩容失败时，栈对象的状态必须原样保留，并返回 `false`。
4. **防御性失败约定**：当栈为空或传入参数无效（空指针）时，`pop`、`top`、`get_min` 必须返回 `false`，且严禁修改输出指针所指向的对象。
5. **完整生命周期**：`min_stack_destroy(NULL)` 必须安全；销毁非空栈时必须释放其持有的全部底层存储资源。

</Exercise>

<Exercise id="31107" :d="4" :w="4">

为不透明字符串键值字典设计独立的只读游标（游标/迭代器 ADT），实现字典封装与遍历机制的彻底解耦：

```c
#include <stdbool.h>
#include <stddef.h>

typedef struct PropertyBag PropertyBag;
typedef struct PropertyCursor PropertyCursor;

PropertyBag *prop_bag_create(void);
void prop_bag_destroy(PropertyBag *bag);
bool prop_bag_put(PropertyBag *bag, const char *key, const char *value);
const char *prop_bag_get(const PropertyBag *bag, const char *key);
size_t prop_bag_size(const PropertyBag *bag);

PropertyCursor *prop_cursor_create(const PropertyBag *bag);
void prop_cursor_destroy(PropertyCursor *cursor);
bool prop_cursor_valid(const PropertyCursor *cursor);
void prop_cursor_next(PropertyCursor *cursor);
const char *prop_cursor_key(const PropertyCursor *cursor);
const char *prop_cursor_value(const PropertyCursor *cursor);
```

要求：

1. **所有权契约**：`PropertyBag` 拥有存入的所有键和值字符串副本（深拷贝）；已有键再次存入时更新其值。
2. **游标与集合分离**：游标对象 `PropertyCursor` 维护对字典的遍历位置；遍历协议不得暴露内部数组或结点的物理指针。
3. **安全遍历协议**：游标使用 `prop_cursor_valid` 判断是否有效，`prop_cursor_next` 单步步进，`prop_cursor_key` 与 `prop_cursor_value` 返回当前位置只读借用指针。
4. **迭代器失效防御（Fail-Fast）**：字典内部维护修改版本计数器（`version`）；若在游标存续期间字典发生修改，游标必须能检测到版本不一致并立即将 `valid` 判定为 `false`，防御脏遍历。
5. **生命周期隔离**：销毁游标仅回收游标自身占用的资源，绝不影响字典对象及内部数据。

</Exercise>

<Exercise id="31108" :d="7" :w="5" scope="**">

在 C 语言中基于虚函数表（Virtual Method Table, VTable）模拟面向对象多态分派机制，设计并实现通用多态字节流抽象数据类型（ByteStream ADT）：

```c
#include <stdbool.h>
#include <stddef.h>

typedef struct ByteStream ByteStream;

typedef struct {
    size_t (*read)(ByteStream *stream, void *buffer, size_t count);
    size_t (*write)(ByteStream *stream, const void *buffer, size_t count);
    bool (*flush)(ByteStream *stream);
    void (*destroy)(ByteStream *stream);
} ByteStreamOps;

struct ByteStream {
    const ByteStreamOps *ops;
};

size_t stream_read(ByteStream *stream, void *buffer, size_t count);
size_t stream_write(ByteStream *stream, const void *buffer, size_t count);
bool stream_flush(ByteStream *stream);
void stream_destroy(ByteStream *stream);

ByteStream *mem_stream_create(void *buffer, size_t capacity);
ByteStream *dyn_stream_create(size_t initial_capacity);
const void *dyn_stream_data(const ByteStream *stream, size_t *out_len);
```

要求：

1. **统一多态分派**：调用方仅持有 `ByteStream *` 指针，所有 I/O 操作通过 `stream_read` 等公开包装函数统一调用，包装函数内部通过 `ops` 虚表路由至具体流实现。
2. **结构体单继承内存对齐**：派生类结构体必须将 `ByteStream base;` 作为其首个成员，确保派生类对象指针与基类指针可安全互转。
3. **定长内存流（`MemStream`）**：封装外部传入的固定大小内存块，维护独立的读写偏移；写入不能超出物理容量上限。
4. **动态自扩容流（`DynStream`）**：拥有自己的堆分配连续存储，写入超出当前容量时按几何倍增自动扩容，并支持通过 `dyn_stream_data` 获取底层有效缓冲视图。
5. **虚析构契约**：调用 `stream_destroy` 时必须通过虚表分派执行具体流特有的析构逻辑，彻底释放派生类持有的所有资源，杜绝内存泄漏。

</Exercise>
