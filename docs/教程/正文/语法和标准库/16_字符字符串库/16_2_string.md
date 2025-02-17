# 字符串库 `<string.h>`

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
      - 如果 `lhs` 先于 `rhs`，返回值为 **负数**；
      - 如果 `lhs` 等于 `rhs`，返回值为 **零**：
      - 如果 `lhs` 后于 `rhs`，返回值为 **正数**。
   3. 示例：

      ```c
      #include <stdio.h>
      #include <string.h>

      void demo(const char* lhs, const char* rhs) {
          const int rc = strcmp(lhs, rhs);
          const char* rel = rc < 0 ? "先于" : rc > 0 ? "后于" : "等于";
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

3. 字符查找 `strchr()`

   1. 定义：

      ```c
      char* strchr( const char* str, int ch );
      ```

   2. 用途：在 `str` 所指的空终止字节字符串中查找字符 `ch`。
   3. 返回值：如果在 `str` 中找到了 `ch`，则返回指向找到的第一个字符的指针，否则返回空指针。
   4. 示例：

      ```c
      #include <stdio.h>
      #include <string.h>

      int main(void) {
          const char *str    = "Try not. Do, or do not. There is no try.";
          char        target = 'o';
          const char *result = str;

          while ((result = strchr(result, target)) != NULL) {
              printf("找到 '%c' 起始于 '%s'\n", target, result);
              ++result; // result 自增，否则我们会找到相同位置的目标
          }
      }
      ```

      可能输出为：

      ```txt
      找到 'o' 起始于 'ot. Do, or do not. There is no try.'
      找到 'o' 起始于 'o, or do not. There is no try.'
      找到 'o' 起始于 'or do not. There is no try.'
      找到 'o' 起始于 'o not. There is no try.'
      找到 'o' 起始于 'ot. There is no try.'
      找到 'o' 起始于 'o try.'
      ```

4. 字符查找 `strrchr()`  
   同上 `strchr()`，只不过查找字符最后一次出现的位置。

5. 子串查找 `strstr()`

   1. 定义：

      ```c
      char* strstr( const char* str, const char* sub );
      ```

   2. 用途：查找字符串 `str` 中首次出现子字符串 `sub` 的位置。
   3. 返回值：如果找到，则返回指向 `str` 中首次出现子字符串 `sub` 的位置的指针；否则返回空指针。
   4. 示例：

      ```c
      #include <string.h>
      #include <stdio.h>

      void find_str(char const* str, char const* substr) {
          char const* pos = strstr(str, substr);
          if(pos == nullptr) {
              printf("没有在 [%s] 中找到 [%s]\n", str, substr);
          } else {
              printf("找到字符串 [%s] 位于 [%s] 的位置 %td\n", substr, str, pos - str);
          }
      }

      int main(void) {
          char const* str = "one two three";
          find_str(str, "two");
          find_str(str, "");
          find_str(str, "nine");
          find_str(str, "n");

          return 0;
      }
      ```

      可能输出为：

      ```txt
      找到字符串 [two] 位于 [one two three] 的位置 4
      找到字符串 [] 位于 [one two three] 的位置 0
      没有在 [one two three] 中找到 [nine]
      找到字符串 [n] 位于 [one two three] 的位置 1
      ```

## 3. 字符串操作

1. 字符串复制 `strcpy()`

   1. 定义：

      ```c
      char* strcpy( char* restrict dest, const char* restrict src );
      ```

   2. 用途：把 `src` 所指的空终止字节字符串复制到 `dest` 所指的字符数组。
   3. 返回值：`dest` 的副本。
   4. 使用此函数的时候，出现以下情况为 UB：
      1. `dest` 所指的字符数组空间不足；
      2. 字符串和字符数组重叠（不满足 `restrict`）；
      3. `dest` 不是指向字符数组的指针 或 `src` 不是指向空终止字节字符串的指针。
   5. 示例：

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

2. 字符串复制 `strncpy()`

   1. 定义：

      ```c
      char* strncpy( char* restrict dest, const char* restrict src, size_t count );
      ```

   2. 用途：把 `src` 所指的字符数组中指定数量的字符复制到 `dest` 所指的字符数组。如果 `src` 所指的字符数组中不足 `count`
      个字符（即在复制字符的数量达到 `count` 之前遇到了空终止字符），则以 `'\0'` 补全。
   3. 返回值：`dest` 的副本。
   4. 使用此函数的时候，出现以下情况为 UB：
      1. 两个字符数组重叠（不满足 `restrict`）；
      2. `dest` 或 `src` 不是指向字符数组的指针；
      3. `src` 所指向的数组大小小于 `count` 且它不含空字符（当然会继续越界访问）；
      4. `dest` 所指的字符数组空间不足。
   5. 示例：

      ```c
      #include <stdio.h>
      #include <string.h>

      int main(void) {
          const char *s1 = "mdr";
          char        s2[strlen(s1)];
          strncpy(s2, s1, 3); // {'m', 'd', 'r'}
          // printf("%s\n", s2);   // 无空终止字符
          char s3[strlen(s1) + 1];
          strncpy(s3, s1, 4); // {'m', 'd', 'r', '\0'}
          printf("%s\n", s3); // OK

          char s4[8];
          // strncpy(s4, s2, 6);   // UB: s2 空间为 3，小于 6
          strncpy(s4, s3, 8); // {'m', 'd', 'r', '\0', '\0',       '\0', '\0', '\0'}

          for (size_t n = 0; n < sizeof s2; ++n) {
              char c = s2[n];
              c ? printf("'%c' ", c) : printf("'\\0' ");
          }
          putchar('\n');

          for (size_t n = 0; n < sizeof s3; ++n) {
              char c = s3[n];
              c ? printf("'%c' ", c) : printf("'\\0' ");
          }
          putchar('\n');

          for (size_t n = 0; n < sizeof s4; ++n) {
              char c = s4[n];
              c ? printf("'%c' ", c) : printf("'\\0' ");
          }
          return 0;
      }

      ```

      可能输出为：

      ```txt
      mdr
      'm' 'd' 'r'
      'm' 'd' 'r' '\0'
      'm' 'd' 'r' '\0' '\0' '\0' '\0' '\0'
      ```
