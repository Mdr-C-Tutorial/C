# const 与 volatile

`const` 与 `volatile` 是 C 的类型限定符 (Type qualifier)。它们修饰的是**类型**，并通过类型影响对对象的访问规则。

## 1. `const`：不可修改

### 1.1 核心语义

当一个对象的类型是 `const` 限定类型时，任何试图修改该对象的行为都会导致未定义行为 (Undefined Behavior, UB)。

```c
const int n = 1;
int* p = (int*)&n;
*p = 2; /* UB */
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


### 1.2 `const` 在指针声明中的位置

这是 C 初学者最容易混淆的部分之一：`const` 永远修饰它“左边最近的类型构造”（若左边没有，就修饰右边）。

```c
int x = 0;

const int* p1 = &x;      /* 指向 const int 的指针：不能通过 *p1 修改 x */
int const* p2 = &x;      /* 与 p1 等价 */
int* const p3 = &x;      /* const 指针：p3 自己不能改指向，但可以通过 *p3 改 x */
const int* const p4 = &x;/* 指向 const int 的 const 指针 */
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


### 1.3 限定符转换

允许从“指向非 const”的指针隐式转换为“指向 const”的指针（添加限定符）；反方向需要显式强转，并且若最终修改了原本为 const 的对象，则是 UB。

## 2. `volatile`：每次访问都具有可观察效果

### 2.1 核心语义（简化表述）

`volatile` 用于告诉编译器：该对象的值可能在程序“看不见”的地方发生变化（例如硬件寄存器、信号处理函数等），因此对该对象的读写应当按抽象机器语义真实发生，不能被随意省略或合并。

同样地：若对象是 `volatile` 限定类型，则试图通过非 volatile 左值去读写它，行为是 UB。

### 2.2 典型使用场景

1. 内存映射 I/O：`static volatile` 对象（或指针）映射硬件寄存器。
2. 与信号处理函数通信：`volatile sig_atomic_t` 标志位。

::: warning `volatile` 不是并发原语

`volatile` 不提供原子性，也不提供线程间的同步与内存序保证。多线程并发请使用 C11 原子与同步原语（见并发章节与 `<stdatomic.h>`）。

:::

## 3. 习题

1. 判断下列声明的含义（写出“指针是否可改”“指向内容是否可改”）：  
   1. `const int* p;`  
   2. `int* const p;`  
   3. `const int* const p;`
2. 写一个“强转去 const”的例子，说明：为什么对原本不是 const 的对象去 const 再写回通常可行，而对原本就是 const 的对象去 const 再写回是 UB。
3. 解释：为什么 `volatile` 不能代替 `<stdatomic.h>`？请给出一个“需要原子性”的并发场景作为反例。
