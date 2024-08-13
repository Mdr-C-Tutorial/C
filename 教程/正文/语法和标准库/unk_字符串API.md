# C 字符串 API `<string.h>`

## 1. 概述

### 1.1 关于 `<string.h>`

`<string.h>` 中提供了很多字符串操作函数和字符串检验函数。

### 1.2 关于字符串

1. 空终止字节字符串 (Null-terminated byte string, NTBS)
2. 空终止多字节字符串 (Null-terminated multibyte string, NTMBS)

### 1.3 字典序

## 2. 字符串检验

1. 字符串长度 `strlen()`
   1. 定义：

      ```c
      size_t strlen( const char* str );
      ```

   2. 用途：计算 `str` 所指的空终止字节字符串的长度（不包括末尾的终止符）。
   3. 示例：

      ```c
      #include <stdio.h>
      #include <string.h>
      
      int main(void) {
          const char *s = "mdr";
          printf("%zu\n", strlen(s));
      }
      ```

      可能输出为 `3`
2. 字符串比较 `strcmp()`
   1. 定义：

      ```c
      int strcmp( const char* lhs, const char* rhs );

      ```

   2. 用途：按照 **字典序** 比较 `lhs` 和 `rhs` 所指的空终止字节字符串。
      + 如果 `lhs` 先于 `rhs`，返回值为 **负数**；
      + 如果 `lhs` 等于 `rhs`，返回值为 **零**：
      + 如果 `lhs` 后于 `rhs`，返回值为 **正数**。
   3. 示例：

      ```c
      #include <stdio.h>
      #include <string.h>
       
      void demo(const char* lhs, const char* rhs) {
          const int rc = strcmp(lhs, rhs);
          const char* rel = rc < 0 ? "先于" : rc > 0 ? "后于      " : "等于";
          printf("[%s] %s [%s]\n", lhs, rel, rhs);
      }
       
      int main(void) {
          const char* string = "Hello World!";
          demo(string, "Hello!");
          demo(string, "Hello");
          demo(string, "Hello there");
          demo("Hello, everybody!" + 12, "Hello, somebody!" + 11);
      }
      ```

      可能输出为：（空格 32，感叹号 33）

      ```txt
      [Hello World!] 先于 [Hello!]
      [Hello World!] 后于 [Hello]
      [Hello World!] 先于 [Hello there]
      [body!] 等于 [body!]
      ```

## 3. 字符串操作

1. 字符串复制 `strcpy()`
   1. 定义：

      ```c
      char* strcpy( char* restrict dest, const char* restrict src );
      ```

   2. 用途：把 `src` 所指的空终止字节字符串复制到 `dest` 所指的字符数组。
   3. 使用此函数的时候，出现以下情况为 UB：
      1. `dest` 所指的字符数组空间不足；
      2. 字符串和字符数组重叠（不满足 `restrict`）；
      3. `dest` 不是指向字符数组的指针 或 `src` 不是指向空终止字节字符串的指针
   4. 示例：

      ```c
      #include <stdio.h>
      #include <string.h>
      
      int main(void) {
          const char *s1 = "mdr";
          // char s2[strlen(s1)]; 数组空间不足
          char s2[strlen(s1) + 1];
          strcpy(s2, s1);
          printf("%s\n", s2);
          return 0;
      }
      ```

      可能输出为 `mdr`
