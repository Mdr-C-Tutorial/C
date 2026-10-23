#ifndef MDR_EXT_H
#define MDR_EXT_H

#include <stddef.h>

/* 21.5：标准库之外、各平台命名不统一的扩展函数 */

/* 分配并复制 s，返回堆上的新串，失败返回 NULL。 */
char *mdr_strdup(const char *s);

/* 最多复制 s 的前 n 个字节并补终止空字符，失败返回 NULL。 */
char *mdr_strndup(const char *s, size_t n);

/* 复制 src 到 dest，返回指向 dest 中终止空字符的指针。 */
char *mdr_stpcpy(char *restrict dest, const char *restrict src);

/* 最多向 dst 复制 dsize - 1 个字符并总是补终止空字符，返回 strlen(src)。
 * 返回值不小于 dsize 表示发生了截断。 */
size_t mdr_strlcpy(char *restrict dst, const char *restrict src, size_t dsize);

/* 向 dst 末尾追加 src，最多写满 dsize 个字节。
 * 返回 strlen(dst) + strlen(src)，不小于 dsize 表示发生了截断。 */
size_t mdr_strlcat(char *restrict dst, const char *restrict src, size_t dsize);

/* 在长度为 haystacklen 的字节序列中查找长度为 needlelen 的子序列。 */
void *mdr_memmem(const void *haystack, size_t haystacklen, const void *needle,
                 size_t needlelen);

/* 可重入的切分函数，进度保存在 *saveptr 中。 */
char *mdr_strtok_r(char *restrict str, const char *restrict delim,
                   char **restrict saveptr);

/* 从 *stringp 中切出下一个字段（保留空字段），并推进 *stringp。 */
char *mdr_strsep(char **restrict stringp, const char *restrict delim);

/* 只按 ASCII 规则不区分大小写地比较字符串，不受区域设置影响。 */
int mdr_ascii_casecmp(const char *s1, const char *s2);
int mdr_ascii_ncasecmp(const char *s1, const char *s2, size_t n);

#endif /* MDR_EXT_H */