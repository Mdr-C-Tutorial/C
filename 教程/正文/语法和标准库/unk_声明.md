# 声明

## 1. 声明由三部分构成：属性说明符序列，说明符与限定符，声明符与初始化器

### 1.1 说明符与限定符

任意顺序的下列内容的空白符分隔列表：

+ 类型说明符：
  + `void`
  + 算术类型名
  + 原子类型名
  + 先前由 `typedef` 声明引入的名称
  + `struct`、`union` 或 `enum` 说明符
  + `typeof` 说明符 (C23 起)
+ 零或一个存储类说明符：`typedef`、`constexpr`、`auto`、`register`、`static`、`extern`、`_Thread_local`
+ 零或多个类型限定符：`const`、`volatile`、`restrict`、`_Atomic`
+ （只在声明函数时）零或多个函数说明符：`inline`、`_Noreturn`
+ 零或多个对齐说明符： `_Alignas`

### 1.2 声明符与初始化器

1. 定义：声明符的**逗号分隔列表**（每个声明符提供附加类型信息和/或要声明的标识符）
2. 声明符**可伴随初始化器**
3. `enum`、`struct` 和 `union` 声明可忽略声明符，这种情况下它们仅引入枚举常量和/或标签。

### 1.3 属性说明符序列

可选的属性列表，应用到被声明的实体

### 1.3 声明符

声明符分为以下五种情况：

1. 标识符 属性说明符序列~opt~
2. ( 声明符 )

3. \* 属性说明符序列~opt~ 限定符~opt~ 声明符
4. 第1/3/4/5种情况的声明符 \[ `static`~opt~ 限定符~opt~ 表达式 \]
   第1/3/4/5种情况的声明符 \[ 限定符~opt~ * \]

5. 第1/3/4/5种情况的声明符 ( 形参或标识符 )

其中，第三种情况为**指针声明符**，第四种情况为**数组声明符**，第五种情况为**函数声明符**

其实就是**把原来声明符的标识符替换成更复杂的东西**，**一层一层**往下替换（很多类似的东西可以这么替换）

可以用英语方便地解释，遇到第三种就在前面加 **pointer to**，第四种加 **array of**，第五种加 **function receives ... returns**
比如 `double (*f[])(int)` ：**double** $\implies$ **function receives int returns double** $\implies$ **pointer to function receives int returns double** $\implies$ **array of pointer to function receives int returns double**

## 2. 例子

### 2.1 `int (*f(void))(void)`

1. 这个声明的 **类型说明符** 是 `int`，**声明符** `(*f(void))(void)` 为第==五==种情况，是一个*函数声明符*，声明一个 **形参列表`void`，返回`int`** 的 **函数**
2. 上一条中所属声明符的 **声明符** 部分 `(*f(void))` 为第==二==种情况
3. 上一条中所属声明符的 **声明符** 部分 `*f(void)` 为第==三==种情况，是一个*指针声明符*，声明一个**指向一个形参列表`void`，返回`int`类型的函数**的**函数指针**
4. 上一条中所属声明符的 **声明符** 部分 `f(void)` 为第==五==种情况，是一个*函数声明符*，声明一个***形参列表`void`***，*返回* ==***指向一个*** **形参列表`void`，返回`int`类型的函数** *的* ***函数指针***== 的 **函数**
5. 上一条中所属声明符的 **声明符** 部分为第==一==种情况，这个函数关联到 **标识符 `f`**

### 2.2 `int (*f(const void *))[3]`

1. 这个声明的 **类型说明符** 是 `int`，**声明符** `(*f(const void *))[3]` 为第==四==种情况，是一个*数组声明符*，声明一个**长度为3，类型为int**的**数组**
2. 上一条中所属声明符的 **声明符** 部分 `(*f(const void *))` 为第==二==种情况
3. 上一条中所属声明符的 **声明符** 部分 `*f(const void *)` 为第==三==种情况，是一个*指针声明符*，声明一个**指向一个长度为3，类型为`int`的数组**的**指针**
4. 上一条中所属声明符的 **声明符** 部分 `f(const void *)` 为第==五==种情况，是一个*函数声明符*，声明一个***接受一个***`const void*`***类型形参***，*返回* ==***指向一个*** **长度为3，类型为`int`的数组** *的**指针***== 的 **函数**
5. 上一条中所属声明符的 **声明符** 部分为第==一==种情况，这个函数关联到 **标识符`f`**

### 2.3 `int(*(*r)(int(*(*)(int(*(*)())()))))[5]`

~~写起来太难了，看注释吧~~

```c
int(*(*r)(int(*(*)(int(*(*)())()))))[5]  // array of int
(*(*r)(int(*(*)(int(*(*)())()))))  // ()
*(*r)(int(*(*)(int(*(*)())())))  // pointer to array of int
(*r)(int(*(*)(int(*(*)())())))  // function returns pointer to array of int
(*r)  // ()
*r  // pointer to function returns pointer to array of int

(int(*(*)(int(*(*)())())))  // parameter list
int(*(*)(int(*(*)())()))  // parameter 1
(*(*)(int(*(*)())())) // ()
*(*)(int(*(*)())())  // pointer to int
(*)(int(*(*)())())  // function returns pointer
(*)  // ()
*  // pointer to function returns pointer to int

(int(*(*)())())  // parameter list
int(*(*)())()  // parameter 1
(*(*)())()  // function returns int
(*(*)())  // ()
*(*)()  // pointer to function returns int
(*)()  // function returns pointer to function returns int
(*)  // ()
*  // pointer to function returns pointer to function returns int

// a pointer to function |- receives a parameter: pointer to function |- receives a parameter: pointer to function |- receives nothing(or unknown before C23)
//                       |                                            |                                            |- returns pointer to function |- reveives nothing(or unknown before C23)
//                       |                                            |                                                                           |- returns int
//                       |                                            |- returns pointer to int
//                       |- returns pointer to array of int
```

## 3. `typedef` 声明

1. 把标识符声明成类型别名，避免类型名过长
2. `typedef` 处于**存储类说明符**的位置，其余和正常声明无区别
3. 例子：

   ```c
   auto    char char1,     *char_p1,    (*f)(void);
   typedef char my_char_t, *my_char_p,  (*fp)(void);
   fp fp2;
   ```

   1. `auto` 是**存储类说明符**，`char1` 具有 `char` 类型，`char_p1` 具有 `char*` 类型，`fp1` 具有 `char (*)(void)` 类型
   2. `typedef` 放在上一句中 `auto` 的位置：`my_char_t` 是 `char` 的别名，`my_char_p` 是 `char*` 的别名，`fp` 是 `char (*)(void)` 的别名
   3. 第三行相当于 `char (*fp2)(void)`
