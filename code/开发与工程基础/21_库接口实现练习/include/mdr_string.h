#ifndef MDR_STRING_H
#define MDR_STRING_H

#include <stddef.h>

/*
 * 返回以空字符结尾的字符串 s 的长度，不含终止空字符。
 * 调用方保证：s 指向以空字符结尾的字符串。
 * 返回：终止空字符之前的字节数。
 */
size_t mdr_strlen(const char *s);

/*
 * 按 unsigned char 的值逐字节比较 lhs 与 rhs。
 * 调用方保证：两者都指向以空字符结尾的字符串。
 * 返回：lhs 较小时为负值，相等时为 0，较大时为正值；不保证具体数值。
 */
int mdr_strcmp(const char *lhs, const char *rhs);

/*
 * 返回 s 开头连续非空字符的个数，最多检查 maxlen 个字节。
 * 调用方保证：s 指向的区域至少有 min(strlen(s) + 1, maxlen) 个可读字节。
 * 返回：范围内遇到空字符时为它之前的字节数，否则为 maxlen；
 *       不读取第 maxlen 个字节之后的内容。
 */
size_t mdr_strnlen(const char *s, size_t maxlen);

/* 21.2：复制与连接 */

/*
 * 把 src（含终止空字符）复制到 dest。
 * 调用方保证：dest 的空间足够容纳整个 src，两个区域不重叠。
 * 返回：dest。
 */
char *mdr_strcpy(char *restrict dest, const char *restrict src);

/*
 * 最多写 count 个字节：src 短于 count 时补空字符到 count 个，
 * 否则 dest 的前 count 个字节不一定是字符串。
 * 调用方保证：dest 至少 count 个可写字节，两个区域不重叠。
 * 返回：dest。源不短于 count 时结果没有终止空字符。
 */
char *mdr_strncpy(char *restrict dest, const char *restrict src, size_t count);

/*
 * 把 src 追加到 dest 末尾（覆盖原终止空字符）并补终止空字符。
 * 调用方保证：dest 能容纳两者连接的结果，两个区域不重叠。
 * 返回：dest。
 */
char *mdr_strcat(char *restrict dest, const char *restrict src);

/*
 * 最多从 src 追加 count 个字符，并总是写入终止空字符。
 * 调用方保证：dest 至少容纳 strlen(dest) + count + 1 个字节。
 * 返回：dest。
 */
char *mdr_strncat(char *restrict dest, const char *restrict src, size_t count);

/* 21.2：比较与查找 */

/* 按 unsigned char 最多比较前 count 个字节，返回值的符号表示大小关系。 */
int mdr_strncmp(const char *lhs, const char *rhs, size_t count);

/* 查找 ch 首次出现的位置；终止空字符也在查找范围内。未找到返回 NULL。 */
char *mdr_strchr(const char *s, int ch);

/* 查找 ch 最后一次出现的位置；未找到返回 NULL。 */
char *mdr_strrchr(const char *s, int ch);

/* 返回 s 开头连续只由 accept 中字符组成的最大前缀长度。 */
size_t mdr_strspn(const char *s, const char *accept);

/* 返回 s 开头连续不含 reject 中任何字符的最大前缀长度。 */
size_t mdr_strcspn(const char *s, const char *reject);

/* 查找 s 中首个出现在 breakset 里的字符，未找到返回 NULL。 */
char *mdr_strpbrk(const char *s, const char *breakset);

/* 查找 needle 在 haystack 中首次出现的位置；needle 为空串时返回 haystack。 */
char *mdr_strstr(const char *haystack, const char *needle);

/* 按 delim 中的字符切分 str，进度保存在内部静态状态中。
 * 调用方保证：不嵌套、不并发地切分两个字符串。 */
char *mdr_strtok(char *restrict str, const char *restrict delim);

#endif /* MDR_STRING_H */