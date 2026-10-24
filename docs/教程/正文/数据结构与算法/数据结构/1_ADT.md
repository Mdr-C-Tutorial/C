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
