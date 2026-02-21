# 抽象数据类型 (Abstract Data Type, ADT)

## 什么是 ADT？

ADT 就像是一个黑盒子 - 我们只关心它能做什么，不关心它内部如何实现。

## 生活中的例子

- 电视机：我们只需要知道怎么换台、调音量，不需要懂内部电路
- 汽车：我们只需要会驾驶，不需要懂发动机原理

## 编程中的 ADT

ADT = 数据 + 操作

### 常见 ADT 示例

1. 栈 (Stack):
   - 操作：push(压入)、pop(弹出)
   - 比喻：像叠盘子，后进先出

2. 队列 (Queue):
   - 操作：enqueue(入队)、dequeue(出队)
   - 比喻：像排队买票，先进先出

## 为什么用 ADT？

1. 隐藏实现细节
2. 提高代码可读性
3. 方便修改内部实现

## C 语言实现示例

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

运行结果：该代码块主要用于展示 ADT 的最小接口形态，单独运行通常无终端输出。

## 抽象层与表示层

上面的栈示例已经体现了 ADT 最重要的思想：调用方只依赖“可以做什么”，而不依赖“内部怎样做”。这一点在 C 语言里尤其重要，因为 C 允许调用方直接接触结构体字段，如果接口设计不收敛，很容易出现“所有调用方都在改内部布局”的情况，后续一旦替换实现，影响面会非常大。更稳妥的做法是把数据表示放进 `.c` 文件，把可见接口收敛到 `.h` 文件，让外部只能通过函数访问对象状态。

## 接口契约应该写清什么

一个可复用的 ADT 接口，至少需要明确四类约束：第一类是对象生命周期，谁负责创建、谁负责销毁；第二类是操作前置条件，例如“空栈不可弹出”；第三类是错误语义，失败时返回什么状态码；第四类是复杂度承诺，例如 `push/pop` 是否保持均摊 O(1)。这几项一旦写清楚，调用方就能在不了解内部细节的前提下稳定使用接口，也能在实现更换时保持行为一致。

## 一个更完整的 C 语言栈 ADT 示例

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

可能的输出（示例）：

<TerminalWindow>

30
20
10

</TerminalWindow>

## 小结

ADT 的价值不在于“把结构体包起来”这一件事，而在于建立一层稳定契约：调用方看到的是行为，维护者管理的是表示。只要契约不变，你就可以把底层从静态数组换成动态数组、从链表换成块链，外部代码通常不需要修改。这种边界清晰的设计方式，会让后续章节里的线性表、栈、队列、树都更容易扩展和验证。
