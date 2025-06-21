# 抽象数据类型 (Abstract Data Type, ADT)

## 什么是 ADT？

ADT 就像是一个黑盒子 - 我们只关心它能做什么，不关心它内部如何实现。

## 生活中的例子

- 电视机：我们只需要知道怎么换台、调音量，不需要懂内部电路
- 汽车：我们只需要会驾驶，不需要懂发动机原理

## 编程中的 ADT

ADT = 数据 + 操作

### 常见 ADT 示例

1. 栈(Stack):
   - 操作：push(压入)、pop(弹出)
   - 比喻：像叠盘子，后进先出

2. 队列(Queue):
   - 操作：enqueue(入队)、dequeue(出队)
   - 比喻：像排队买票，先进先出

## 为什么用 ADT？

1. 隐藏实现细节
2. 提高代码可读性
3. 方便修改内部实现

## C 语言实现示例

```c
// 栈的ADT定义
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
