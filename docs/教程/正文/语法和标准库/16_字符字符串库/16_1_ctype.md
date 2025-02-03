# C 字符处理库 `<ctype.h>`

## 1. 字符分类

在 `<ctype.h>` 中，定义了很多判断字符类型的函数，在本地环境中行为如下：

1. **`isalnum()`:** 判断字符是否为字母或数字 (A-Z, a-z, 0-9)。如果是，返回非零值；否则返回 0。
2. **`isalpha()`:** 判断字符是否为字母 (A-Z, a-z)。如果是，返回非零值；否则返回 0。
3. **`islower()`:** 判断字符是否为小写字母 (a-z)。如果是，返回非零值；否则返回 0。
4. **`isupper()`:** 判断字符是否为大写字母 (A-Z)。如果是，返回非零值；否则返回 0。
5. **`isdigit()`:** 判断字符是否为数字 (0-9)。如果是，返回非零值；否则返回 0。
6. **`isxdigit()`:** 判断字符是否为十六进制数字 (0-9, a-f, A-F)。如果是，返回非零值；否则返回 0。
7. **`isspace()`:** 判断字符是否为空格符（空格、水平制表符`\t`、换行符`\n`、垂直制表符`\v`、换页符`\f`、回车符`\r`）。如果是，返回非零值；否则返回 0。
8. **`iscntrl()`:** 判断字符是否为控制字符 (ASCII 码 0-31 和 127，包括 null 字符 `\x00` 和 delete 字符 `\x7F` )。如果是，返回非零值；否则返回 0。
9. **`ispunct()`:** 判断字符是否为标点符号 (例如：``! " # $ % & ' ( ) * + , - . / : ; < = > ? @ [ \ ] ^ _ ` { | } ~``)。如果是，返回非零值；否则返回 0。
10. **`isblank()`:** 判断字符是否为空格符（空格或水平制表符 `\t`）。如果是，返回非零值；否则返回 0。
11. `isgraph()` 判断字符有没有图形表示，即 ``0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!"#$%&'()*+,-./:;<=>?@[\]^_`{|}~`` 返回非零值，否则返回 **0**。简单地，对于 ASCII 表中的字符 `c`，如果 `iscntrl(c) == 0 && isblank(c) == 0`，那么 `isgraph(c)` 返回 非零值，否则返回 **0**

::: tip 非零值

我们一般不会直接用到上述函数的返回值，而是直接将其用在条件判断中，如 `if (isalnum(c)) { ... } else { ... }`，因此只需区别返回值是否为零。

:::

::: note wctype.h

在 `<wctype.h>` 中，也有这样的函数，将上述函数名中的 'is' 后面加上 'w'，如 `iswalnum()` 判断一个宽字符是不是字母或数字，等等。。。

:::

如果设置了非默认的本地环境，以上函数的运行结果会有不同。

示例代码：

```c
#include <ctype.h>
#include <stdio.h>

int main() {
    char characters[] =
        {
            'A', 'a', 'Z',  'z',  '0',  '9',  '!',  '?',  '#',
            '$', ' ', '\t', '\n', '\r', '\v', '\f', '\0', '\x7F',
            '*', '/', '[',  '^',  '{',  '~',  ':',  ';',  '<',
            '=', '>', '@',  '\\', '`',  '|',  '%'}; 

    int num = sizeof(characters) / sizeof(characters[0]);

    printf(
        "Character   isalnum   isalpha   islower   isupper   isdigit   "
        "isxdigit  isspace   iscntrl   ispunct   isblank   isgraph\n");
    printf(
        "----------  --------  --------  --------  --------  --------  "
        "--------  --------  --------  --------  --------  ---------\n");

    for (int i = 0; i < num; i++) {
        char c = characters[i];
        if (isgraph(c)) {
            printf("  %c     ", c);
        } else {
            if (c == '\n') {
                printf("  \\n    ", c);
            } else if (c == '\r') {
                printf("  \\r    ", c);
            } else if (c == '\t') {
                printf("  \\t    ", c);
            } else if (c == '\v') {
                printf("  \\v    ", c);
            } else if (c == '\f') {
                printf("  \\f    ", c);
            } else if (c == ' ') {
                printf(" [SPACE]", c);
            } else if (c == '\0') {
                printf("  \\0    ", c);
            } else if (c == '\x7F') {
                printf(" [DEL]  ", c);
            } else if (c == ' ') {
                printf(" [SPACE]", c);
            }
        }
        printf("%10d%10d%10d%10d%10d%10d%10d%10d%10d%10d%10d\n", isalnum(c),
               isalpha(c), islower(c), isupper(c), isdigit(c), isxdigit(c),
               isspace(c), iscntrl(c), ispunct(c), isblank(c), isgraph(c));
    }

    return 0;
}
```

## 2. 大小写转换

在 `<ctype.h>` 中，定义了 **大小写转换** 的函数，如下：

1. `toupper()` 将字符转换为大写；
2. `tolower()` 将字符转换为小写。

::: note wctype.h

在 `<wctype.h>` 中，也有这样的函数，将上述函数名中的 'to' 后面加上 'w'，如 `towupper()` 将宽字符转换为大写。

:::

**本章内容对应 cppref 链接如下：**

- [空终止字节字符串](https://zh.cppreference.com/w/c/string/byte)
- [字符分类](https://zh.cppreference.com/w/c/string/byte#.E5.AD.97.E7.AC.A6.E5.88.86.E7.B1.BB)
- [宽字符分类](https://zh.cppreference.com/w/c/string/wide#.E5.AD.97.E7.AC.A6.E5.88.86.E7.B1.BB)
