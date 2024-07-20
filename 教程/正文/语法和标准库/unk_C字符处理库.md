# C 字符处理库 `ctype.h` `wctype.h`

## 1. 字符分类

在 `ctype.h` 中，定义了很多 **判断字符类型** 的函数，如下：

1. `isalnum()` 判断是不是字母或数字

在 `wctype.h` 中，也有这样的函数，将上述函数名中的 'is' 后面加上 'w'，如 `iswalnum()` 判断是不是字母或数字。

## 2. 大小写转换

在 `ctype.h` 中，定义了 **大小写转换** 的函数，如下：

1. `toupper()` 将字符转换为大写
2. `tolower()` 将字符转换为小写

在 `wctype.h` 中，也有这样的函数，将上述函数名中的 't' 后面加上 'w'，如 `towupper()` 将字符转换为大写。

**本章内容对应 cppref 链接如下：**

+ [空终止字节字符串](https://zh.cppreference.com/w/c/string/byte)
