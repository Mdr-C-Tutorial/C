# constexpr（C23）

`constexpr` 是 C23 引入的存储类说明符 (Storage-class specifier)。用 `constexpr` 声明的**标量对象** (Scalar object) 是一个“常量对象”：它在运行期存在（可以取地址），但其值在翻译期 (Translation time) 就固定下来，且运行期不可被修改。

## 1. 核心语义

对 `constexpr` 对象，标准要求：

1. 必须**完全且显式地初始化**，并且初始化遵循“静态初始化” (Static initialization) 的规则；
2. 初始化器所用的常量表达式会在编译期检查；
3. 对浮点类型初始化器，必须在“翻译期浮点环境”下求值（避免被运行期舍入模式影响）。

它仍然拥有由其声明决定的链接 (Linkage)，并且在运行时存在（因此可以取地址）；只是运行时不允许修改它。

## 2. 限制

并非所有类型都允许使用 `constexpr`。标准对可用类型施加了限制，常见的限制包括：

1. 只允许标量对象（因此数组、结构体等不适用）。
2. 指针类型仅允许空指针常量 (Null pointer constant) 作为 `constexpr`（即只能是“空指针”这一类值）。
3. 不允许可变修改类型 (VLA 相关)、原子类型、`volatile` 类型、`restrict` 指针等。

## 3. `const` 与 `constexpr` 的区别

1. `const` 只保证“不能通过 const 限定的左值修改对象”；它并不保证该值是编译期常量。
2. `constexpr` 额外保证：该对象的值可在常量表达式中使用，并且初始化器在编译期可验证。

## 4. 示例

```c
#include <stdio.h>

constexpr int answer = 42;
static_assert(answer == 42);

int main(void) {
    printf("%d\n", answer);
    return 0;
}
```

## 5. 习题

1. 写出一个 `constexpr int`，并分别把它用于：
   1. `static_assert`；
   2. `switch` 的 `case` 标签；
   3. 位域宽度（若你的编译器支持）。
2. 解释：为什么 `const int x = 42;` 不能保证用于 `case` 标签，而 `constexpr int x = 42;` 可以？
3. 写出若干个“非法的 constexpr 声明”，并解释它们分别违反了哪条限制（至少包含：指针非空、`volatile`、原子类型、VLA）。
