/* textconv.h：UTF-8 与 UTF-16 互转。本页的示例与习题共用。 */
#ifndef TEXTCONV_H
#define TEXTCONV_H

#include <wchar.h>

/* UTF-8 -> UTF-16。成功返回 malloc 出来的缓冲区（含结尾空字符），调用者 free；
   输入不是合法 UTF-8 或内存不足时返回 NULL。 */
wchar_t *utf8_to_wide(const char *utf8);

/* UTF-16 -> UTF-8。成功返回 malloc 出来的缓冲区（含结尾空字符），调用者 free；
   输入含孤立代理项或内存不足时返回 NULL。 */
char *wide_to_utf8(const wchar_t *wide);

#endif