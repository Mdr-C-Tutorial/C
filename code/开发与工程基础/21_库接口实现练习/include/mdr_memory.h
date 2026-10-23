#ifndef MDR_MEMORY_H
#define MDR_MEMORY_H

#include <stddef.h>

/*
 * 把 ch 转换成 unsigned char 后写入 dest 指向区域的前 count 个字节。
 * 调用方保证：dest 指向至少 count 个可写字节。
 * 返回：dest。
 */
void *mdr_memset(void *dest, int ch, size_t count);

/*
 * 从 src 复制 count 个字节到 dest。
 * 调用方保证：两个区域都至少有 count 个字节，且互不重叠。
 * 返回：dest。
 */
void *mdr_memcpy(void *restrict dest, const void *restrict src, size_t count);

/* 21.3：内存函数 */

/*
 * 从 src 复制 count 个字节到 dest，允许两个区域重叠。
 * 调用方保证：两个区域都至少有 count 个可写、可读的字节。
 * 返回：dest。
 */
void *mdr_memmove(void *dest, const void *src, size_t count);

/*
 * 按 unsigned char 比较两个区域的前 count 个字节。
 * 返回：lhs 较小时为负值，相等时为 0，较大时为正值；不保证具体数值。
 */
int mdr_memcmp(const void *lhs, const void *rhs, size_t count);

/*
 * 在前 count 个字节中查找首次出现的 (unsigned char)ch，遇到空字符不停止。
 * 返回：指向该字节的指针；没找到时返回 NULL。按 void * 返回，调用方自己转换。
 */
void *mdr_memchr(const void *ptr, int ch, size_t count);

/*
 * 从 src 向 dest 复制，遇到 (unsigned char)c 或复制满 count 个字节为止。
 * 调用方保证：两个区域各至少有 count 个字节，互不重叠。
 * 返回：复制到 c 时返回 dest 中紧跟该字节的指针，否则返回 NULL。
 */
void *mdr_memccpy(void *restrict dest, const void *restrict src, int c,
                  size_t count);

/*
 * 与 mdr_memset 相同，但保证编译器不会把这次写入当成无用操作删除。
 * 返回：dest。用于清除密码、密钥一类不应留在内存里的数据。
 */
void *mdr_memset_explicit(void *dest, int ch, size_t count);

#endif /* MDR_MEMORY_H */