# 字符串库 `<string.h>`

# 第一类：最最常用的字符串处理函数
## `strlen()` ：字符串长度计算
### 说文解字
`str` 是 ”string“ 的缩写
`len` 是 ”length” 的缩写
`strlen` 即 length of string，它的作用是计算字符串长度
### 函数原型
**`size_t strlen(const char *s)`**
`s`：是要计算长度的字符串
返回字符串`s`的长度
### 注意事项
这个函数的使用要区别于`sizeof()`运算符计算数组长度的方法
有一种计算数组长度的方法是使用`sizeof()`计算数组占用的总内存，除以单个数据元素占用的内存空间得到数组长度，即`sizeof(array) / sizeof(type)`
在字符串这里你会发现使用`strlen()`函数和使用以上方法的结果不同，原因在于C语言中，字符串以空字符(Nul Character, ASCII: 0)为结束标志。`strlen()`函数只统计字符串中非空字符的数量，而`sizeof`运算符会把空字符也算上，请注意这个特殊的地方。
### 衍生函数
`size_t strnlen(const char *s, size_t maxlen)`
`strlen()`的字符数限制版，返回字符串 `s` 的长度，但最多返回 `maxlen`
### 示例程序
``` C
#include <stdio.h>
#include <string.h>

int main() {
    char str[] = "Hello, world!";
    int length = strlen(str);

    printf("The length of the string \"%s\" is %d.\n", str, length);

    return 0;
}
```
输出：
> The length of the string "Hello, world!" is 13.

## `strcmp()`：字符串比较
### 说文解字
`str` 是 `string`的缩写
`cmp` 是 `compare`的缩写
`strcmp` 即 compare string，比较字符串
### 函数原型
`int strcmp(const char *s1, const char *s2)`
`s1`、`s2`：要比较的两个字符串常量
### 注意事项
注意这个函数的返回值
常识当中，对于字符串只有相同与不同之分，给字符串分出来一个大小是比较反直觉的。
但这个函数能做到
>概念引入：字典序  
>字典序（Lexicographical Order）是一种基于字符或元素顺序的比较方法，类似于字典中单词的排序规则。它广泛应用于字符串、数组、序列等的比较和排序中。字典序的核心思想是按照从左到右的顺序逐个比较元素，直到找到第一个不同的位置，从而确定两个序列的大小关系。

说人话就是，在比到不同字符之前会一直往下比，如果所有字符都相同，就返回0，如果有一个字符不同，就停止比较，返回一个数字表示这两个字符字典序的关系（大于就返回正数，小于就负数，等于就是0）。
基于此原理，我们就可以给字符串排序了
但是实际使用中，我们用它来匹配比较多，所以有一个常用写法：
``` C
if (!strcmp(str1, str2))
{
	...
}
```
### 衍生函数
1. `int strcasecmp(const char *str1, const char *str2)`
`strcmp()`的大小写不敏感（case insensitive）版
2. `int strncmp(const char *s1, const char *s2, size_t n)`
`strcmp()`的字符数限制版，只比较前n个字符
### 示例程序
``` C
#include <stdio.h>
#include <string.h>

int main() {
    char str1[] = "Hello";
    char str2[] = "World";
    char str3[] = "Hello";

    int result1 = strcmp(str1, str2);
    int result2 = strcmp(str1, str3);

    printf("Comparing '%s' and '%s': %d\n", str1, str2, result1);
    printf("Comparing '%s' and '%s': %d\n", str1, str3, result2);

    return 0;
}
```
输出：
> Comparing 'Hello' and 'World': -1
> Comparing 'Hello' and 'Hello': 0
## `strcpy()`：字符串复制函数
### 说文解字
`cpy`是“copy”的缩写，建议直接读成“string copy”
### 函数原型
`char *strcpy(char *dest, const char *src)`
`src`：被复制的字符串
`dest`：目标字符串
这个函数将字符串`src`复制到字符串`dest`的地址中，并返回指向`dest`的指针
### 注意事项
这个函数使用时很可能会发生越界问题<br>
孩子们，这并不好笑  
使用这个函数时，程序员要自己对程序的安全负责，检查字符串的长度
### 衍生函数
1. `char *stpncpy(char *dest, const char *src, size_t n)`
将 `src` 的前 `n` 个字符复制到 `dest`，返回指向 `dest` 的指针
2. `size_t strlcpy(char *dest, const char *src, size_t size)` 
安全地将 `src` 复制到 `dest`，最多复制 `size - 1` 个字符。
>当`src`长度>=size时，`strncpy`不添加空字符，而`strlcpy`会添加
`strlcpy()`：返回源字符串的长度，方便检查截断
`strncpy()`：返回目标字符串长度
### 示例函数
``` C
#include <stdio.h>
#include <string.h>

int main() {
    char source[] = "Hello, World!";
    char destination[50];

    // Copy the contents of source to destination
    strcpy(destination, source);

    // Print the copied string
    printf("Copied string: %s\n", destination);

    return 0;
}
```
输出：
> Copied string: Hello, World!
## `strcat()`：字符串连接
### 说文解字
`cat`是 “concatenate” 的缩写，意思是连接或合并。
### 函数原型
`char *strcat(char *dest, const char *src)`
`src`：源字符串
`dest`：目标字符串
将 `src` 连接到 `dest` 的末尾，返回指向`dest`的指针。
### 注意事项
同上，程序员要自己负责安全问题，使用时要考虑到字符串长度。
### 衍生函数
1. `char *strncat(char *dest, const char *src, size_t n)`
将 `src` 的前 `n` 个字符连接到 `dest` 的末尾，返回指向`dest`末尾的指针。
2. `size_t strlcat(char *dest, const char *src, size_t size)`
安全地将 `src` 连接到 `dest`，最多写入 `size - 1` 个字符。
### 示例程序
``` C
#include <stdio.h>
#include <string.h>

int main() {
    char str1[100] = "Hello, ";
    char str2[] = "world!";

    strcat(str1, str2);

    printf("Resulting string: %s\n", str1);

    return 0;
}
```
输出：
> Resulting string: Hello, world!
# 第二类：字符串解析函数
## `strchr()`：在字符串中定位特定字符
### 说文解字
`chr` 是 “char” 也就是 “character” 的缩写，可以把它理解成在"str"中寻找一个"char"。
### 函数原型
`char *strchr(const char *s, int c)`
`s`：被查找的字符串
`c`：要匹配的字符
查找字符 `c` 在字符串 `s` 中的第一次出现，返回字符串中首次出现字符`c`的指针，如果未找到字符，则返回`NULL`。
### 衍生函数
`char *strrchr(const char *s, int c)`
查找字符 `c` 在字符串 `s` 中的最后一次出现，其中`r`可以理解为“reverse”，这个函数相当于是反向遍历版的strchr()
### 示例程序
``` C
#include <stdio.h>
#include <string.h>

int main() {
    const char *str = "Hello, world!";
    char ch = 'w';
    char *result = strchr(str, ch);

    if (result != NULL) {
        printf("Character '%c' found at position: %ld\n", ch, result - str);
    } else {
        printf("Character '%c' not found.\n", ch);
    }

    return 0;
}
```
输出：
> Character 'w' found at position: 7
## `strstr()`：在字符串中查找字符串
### 说文解字
没什么好说的了，在“str”里查找“str”
### 函数原型
`char *strstr(const char *s1, const char *s2)`
`s1`：被查找的字符串
`s2`：要匹配的字符串
查找子字符串 `s2` 在字符串 `s1` 中的位置
### 示例程序
``` C
#include <stdio.h>
#include <string.h>

int main() {
    const char haystack[] = "Hello, welcome to the world of programming.";
    const char needle[] = "world";
    char *result;

    result = strstr(haystack, needle);

    if (result) {
        printf("Substring found at position: %ld\n", result - haystack);
    } else {
        printf("Substring not found.\n");
    }

    return 0;
}
```
输出：
> Substring found at position: 22
## `strtok()`：用某些字符分割字符串
### 说文解字
`tok`：可以理解成“tokenize”的缩写，是“分词”的意思，它把一个字符串分解成若干个单词
### 函数原型
`char *strtok(char *s, const char *delim)`
`s`：被分割的字符串
`delim`：分割符集合，是一个包含所有分隔符的字符串
将字符串 `s` 按分隔符 `delim` 分割成多个子字符串，并返回指向第一个子字符串的指针。其中，`delim`是一个字符串，里面包含了所有用来分割字符串的字符。如果没有可供分割的子字符串了，则返回NULL。
### 注意事项
1. 如果你要将一个字符串分割成多个词元的话，分割第二个词元时要注意一下参数的写法，如果你还写那个目标字符串的话，分割出来的词元还是第一个，正确的写法应该是`char *token = strtok(NULL, delim);`注意字符串参数传一个空指针进去就行。
2. 这个函数是在原字符串上直接修改的，具体实现为将分隔符置为空字符来标识子字符串的末尾，如果需要原字符串保持不变的，程序员应当提前保存。
### 衍生函数
`char *strtok_r(char *s, const char *delim, char **ptr)`  
线程安全版本的 `strtok`，其中，`char **ptr`是一个指针的指针，用来记录上一次分割的位置
### 示例程序
``` C
#include <stdio.h>
#include <string.h>

int main() {
    char str[] = "Hello, world! This is a strtok example.";
    char delim[] = " ,.!";

    // 获取第一个标记
    char *token = strtok(str, delim);

    // 遍历其他标记
    while (token != NULL) {
        printf("%s\n", token);
        token = strtok(NULL, delim);
    }

    return 0;
}
```
输出：
> Hello
> world
> This
> is
> a
> strtok
> example
## `strpbrk()`：在字符串中查找字符集中字符的位置
### 说文解字
`p`：“pointer”的缩写
`brk`：“break”的缩写
意思就是查找字符集中的任一字符在字符串中的首次出现位置
### 函数原型
`char *strpbrk(const char *s1, const char *s2)`
`s1`：目标字符串
`s2`：字符集字符串，包含要查找的字符集合
如果查找到匹配字符，返回指向它的指针，否则返回`NULL`
``` C
#include <stdio.h>
#include <string.h>

int main() {
    const char haystack[] = "Hello, welcome to the world of programming.";
    const char needle[] = "aeiou"; // Example set of characters to search for
    char *result;

    result = strpbrk(haystack, needle);

    if (result) {
        printf("First matching character found at position: %ld\n", result - haystack);
    } else {
        printf("No matching characters found.\n");
    }

    return 0;
}
```
输出：
> First matching character found at position: 1
## `strspn()`：计算一个字符串中包含另一个字符串中字符的最长连续子串的长度
### 说文解字
`spn`：“span”的缩写
可以理解为“计算字符串中某个范围的长度”
### 函数原型
`char *strspn(const char *s1, const char *s2)`
`s1`：目标字符串，也就是需要检查的字符串
`s2`：字符集字符串，包含需要匹配的字符集合。
返回字符串 `s1` 中仅包含 `s2` 中字符的连续子串的长度，具体地说，是从 `s1` 的开头开始，计算最长的连续子串的长度，该子串中的所有字符都必须属于 `s2`，一旦遇到 `s1` 中不属于 `s2` 的字符，计数停止。
### 衍生函数
`char *strcspn(const char *s1, const char *s2)`
多出来的那个`c`可以理解为“complement”，意为“补集”，表示的是查找字符串`s1`中，**不包含**字符串`s2`中字符的最大长度
### 示例程序
``` C
#include <stdio.h>
#include <string.h>

int main() {
    const char *str1 = "abcde312$#@";
    const char *str2 = "abcde";

    size_t len = strspn(str1, str2);

    printf("The initial segment of str1 containing only characters from str2 is %zu characters long.\n", len);

    return 0;
}
```
输出：
> The initial segment of str1 containing only characters from str2 is 5 characters long.
# 第三类：内存操作函数
## `memcpy()`：复制指定长度的内存区域到指定位置
### 说文解字
`mem`：“memory”的缩写，意为“内存”
其实就是“memory copy”，复制内存
### 函数原型
`void *memcpy(void *dest, const void *src, size_t n)`
`dest`：目标内存的起始地址
`src`：源内存的起始地址
`n`：要复制的字节数
返回值：`dest`
### 注意事项
1. 若内存区域有重叠，`memcpy`的行为是未定义的，谨慎使用
2. 尽管可以用于任意类型的复制，但是要确保`dest`和`src`的类型兼容
3. `n`的值决定了有可能存在数组越界的问题，孩子们，这并不好笑
### 示例程序
``` C
#include <stdio.h>
#include <string.h>

int main() {
    char src[50] = "Hello, World!";
    char dest[50];

    // Copy contents of src to dest
    memcpy(dest, src, strlen(src) + 1);

    printf("Source: %s\n", src);
    printf("Destination: %s\n", dest);

    return 0;
}
```
输出：
>Source: Hello, World!
>Destination: Hello, World!
## `memmove()`：移动指定长度的内存区域到指定位置
### 说文解字
这真没啥好说的了吧
### 函数原型
`void *memmove(void *dest, const void *src, size_t n)`
`dest`：目标内存的起始地址。
`src`：源内存的起始地址。
`n`：要移动的字节数。
返回`dest`
### 注意事项
同上，但是不用考虑内存重叠的问题了
>这里多提一嘴为什么`memmove()`可以不用考虑内存重叠的问题：
>`memmove`内有检查机制，如果出现内存区域重叠，它会从后向前复制从而避免未复制的值被覆盖，如果没有重叠，它的实现机制和`memcpy()`是一样的。
>那`memcpy()`存在的意义是什么？
>`memcpy()`不需要进行额外的逻辑检查，执行速度快于`memmove()`
### 示例程序
``` C
#include <stdio.h>
#include <string.h>

int main() {
    char str1[] = "Hello, World!";
    char str2[50];

    // Using memmove to copy str1 to str2
    memmove(str2, str1, strlen(str1) + 1);

    printf("str2: %s\n", str2);

    // Overlapping memory regions
    char str3[] = "memmove can be very useful......";
    memmove(str3 + 20, str3 + 15, 11);

    printf("str3: %s\n", str3);

    return 0;
}
```
输出：
> str2: Hello, World!
> str3: memmove can be very very useful.
## `memset()`：设置指定长度的内存区域为指定的值
### 说文解字
这也没啥好说的了吧，`set`：“设置”
### 函数原型
`void *memset(void *s, int c, size_t n)`
`s`：目标内存的起始地址。
`c`：要设置的值（以 `int` 类型传入，但实际按字节处理）。
`n`：要设置的字节数。
返回目标内存的起始地址（`s`）
### 注意事项
1. 虽然第二个参数是整型，但实际上是按照字节来操作的，也就是会被截断为无符号字符型
2. `n`，有可能数组越界
3. 确保类型的兼容性
### 示例程序
``` C
#include <stdio.h>
#include <string.h>

int main() {
    char buffer[25];

    // Fill buffer with '-' characters
    memset(buffer, '-', sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0'; // Null-terminate the string

    printf("Buffer after memset: %s\n", buffer);

    return 0;
}
```
输出：
> Buffer after memset: ------------------------
## `memcmp()`：内存区域比较函数
### 说文解字
孩子们，这并不抽象
### 函数原型
`int memcmp(const void *s1, const void *s2, size_t n)`
`s1`：指向第一个内存区域的指针
`s2`：指向第二个内存区域的指针
`n`：要比较的字节数
返回值类似`strcmp()`，也是ASCII码的差值。
### 注意事项
1. `memcmp()` 是按字节进行比较的，因此比较的是内存中的原始字节值。它不会考虑数据的类型（如整数、浮点数等），因此比较的结果可能与预期不同，尤其是当内存中存储的是非字节对齐的数据时。     
2. 数组越界：孩子们，想我了吗？
3. 如果比较的内存区域包含未初始化的值，结果是未定义的。
### 示例程序
``` C
#include <stdio.h>
#include <string.h>

int main() {
    char str1[] = "Hello, World!";
    char str2[] = "Hello, World!";
    char str3[] = "Hello, C Programming!";

    // Compare str1 and str2
    int result = memcmp(str1, str2, sizeof(str1));
    if (result == 0) {
        printf("str1 and str2 are equal.\n");
    } else {
        printf("str1 and str2 are not equal.\n");
    }

    // Compare str1 and str3
    result = memcmp(str1, str3, sizeof(str1));
    if (result == 0) {
        printf("str1 and str3 are equal.\n");
    } else {
        printf("str1 and str3 are not equal.\n");
    }

    return 0;
}
```
输出：
>str1 and str2 are equal.
>str1 and str3 are not equal.
## `memchr()`：在指定内存区域查找指定字符
### 说文解字
孩子们，前面都说过了
### 函数原型
`void *memchr(const void *s, int c, size_t n)`
**`s`**：指向要搜索的内存块的指针。
**`c`**：要搜索的字符，虽然以 `int` 类型传入，但在搜索时会被转换为 `unsigned char`。
**`n`**：指定搜索的字节数。
如果找到字符 `c`，返回指向该字符的指针（类型为 `void*`）。
如果未找到字符 `c`，返回 `NULL`
### 注意事项
数组越界：孩子们，还是我，想我了吗
### 示例程序
``` C
#include <stdio.h>
#include <string.h>

int main() {
    const char str[] = "Hello, world!";
    const char ch = 'w';
    char *result;

    result = memchr(str, ch, strlen(str));

    if (result != NULL) {
        printf("Character '%c' found at position: %ld\n", ch, result - str);
    } else {
        printf("Character '%c' not found.\n", ch);
    }

    return 0;
}
```
输出：
> Character 'w' found at position: 7

# 写在最后
这个文档里面只列举了我用过的函数，有些我实在是没用过的，我也没有什么自己的理解，就没有写进来，我把他们在下面列出来：
`strcoll()`、`strxfrm()`、`strerror()`、`strsignal()`
感兴趣的话请各位读者自行查阅资料。