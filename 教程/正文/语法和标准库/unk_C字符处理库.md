# C 字符处理库 `<ctype.h>` `<wctype.h>`

## 1. 字符分类

在 `<ctype.h>` 中，定义了很多 **判断字符类型** 的函数，在 **本地环境** 中行为如下：

1. `isalnum()` 判断是不是字母或数字，即 (ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789)返回 **非零值**，否则返回 **0**
2. `isalpha()` 判断是不是字母，即 `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz` 返回 **非零值**，否则返回 **0**
3. `islower()` 判断是不是小写字母，即 `abcdefghijklmnopqrstuvwxyz` 返回 **非零值**，否则返回 **0**
4. `isupper()` 判断是不是大写字母，即 `ABCDEFGHIJKLMNOPQRSTUVWXYZ` 返回 **非零值**，否则返回 **0**
5. `isdigit()` 判断是不是数字，即 `0123456789` 返回 **非零值**，否则返回 **0**
6. `isxdigit()` 判断是不是十六进制数字，即 `0123456789abcdefABCDEF` 返回 **非零值**，否则返回 **0**
7. `isspace()` 判断是不是空格，即 `[SPACE]` 和 `\t\n\v\f\r` 返回 **非零值**，否则返回 **0**
8. `iscntrl()` 判断是不是控制字符，即 `[\x00-\x1F\x7F]` 返回 **非零值**，否则返回 **0**
9. `ispunct()` 判断是不是标点符号，即 `[\x21-\x2F\x3A-\x40\x5B-\x60\x7B-\x7E]` 返回 **非零值**，否则返回 **0**
10. `isblank()` 判断是不是空格，即 `[SPACE]` 和 `\t`（水平制表符） 返回 **非零值**，否则返回 **0**
11. `isgraph()` 判断字符有没有图形表示，即 ``0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!"#$%&'()*+,-./:;<=>?@[\]^_`{|}~`` 返回 **非零值**，否则返回 **0**
    对于 ASCII 表中的字符 `c`，如果 `iscntrl(c) == 0 && isblank(c) == 0`，那么 `isgraph(c)` 返回 **非零值**，否则返回 **0**

在 `<wctype.h>` 中，也有这样的函数，将上述函数名中的 'is' 后面加上 'w'，如 `iswalnum()` 判断一个宽字符是不是字母或数字，等等。。。

如果设置了非默认的本地环境，以上函数的运行结果会有不同

## 2. 大小写转换

在 `<ctype.h>` 中，定义了 **大小写转换** 的函数，如下：

1. `toupper()` 将字符转换为大写
2. `tolower()` 将字符转换为小写

在 `<wctype.h>` 中，也有这样的函数，将上述函数名中的 'to' 后面加上 'w'，如 `towupper()` 将宽字符转换为大写。

**本章内容对应 cppref 链接如下：**

+ [空终止字节字符串](https://zh.cppreference.com/w/c/string/byte)
