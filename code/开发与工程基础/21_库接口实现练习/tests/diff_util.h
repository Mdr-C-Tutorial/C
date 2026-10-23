#ifndef DIFF_UTIL_H
#define DIFF_UTIL_H

#include <stddef.h>
#include <stdint.h>

/*
 * 差分测试的公共工具：规范化比较、失败报告、防护缓冲区、随机数与输入缩小。
 * 规范化把两份实现之间允许的差异消掉，只留下行为上的真正差别。
 */

/* 把任意整数规范化成 -1、0 或 1，只保留符号。 */
int diff_sign(int value);

/* 把查找函数返回的指针换成相对 base 的偏移；NULL 返回 -1。 */
long diff_offset(const void *base, const void *found);

/* 把字节序列成带引号的文本，不可打印的字节写成 \xNN。 */
void diff_escape(char *out, size_t cap, const void *data, size_t len);

/* 打印一行十六进制转储，最多 32 个字节。 */
void diff_dump(const char *label, const void *data, size_t len);

/* 计一次通过的比较。 */
void diff_ok(void);

/*
 * 报告一次不一致：打印说明、输入与期望/实际值，并计入失败数。
 * 前 5 次打印细节，之后只计数，免得长输出淹没终端。返回 1。
 */
int diff_report(const char *what, const void *input, size_t input_len,
                long expected, long actual);

/* 打印「套件: 总数 comparisons, 失败数 failed」，有失败返回 1。 */
int diff_summary(const char *suite);

/* 防护字节的个数：数据区前后各这么多字节。 */
#define DIFF_GUARD 16

typedef struct {
    unsigned char *raw;
    unsigned char *data;
    size_t size;
} diff_guard;

/* 分配 size 字节的数据区，前后各带 DIFF_GUARD 个 0xA5。成功返回 0。 */
int diff_guard_init(diff_guard *guard, size_t size);
void diff_guard_free(diff_guard *guard);

/* 检查哨字节是否完好；被改写时打印位置与内容，返回 1。 */
int diff_guard_check(const diff_guard *guard, const char *what);

/* splitmix64：状态由调用方保存，同一个种子给出同一串数。 */
uint64_t diff_random(uint64_t *state);

/* 解析 --seed 的实参，支持十进制与 0x 前缀；格式不对时退出。 */
uint64_t diff_parse_seed(const char *text);

/*
 * 缩小失败输入：反复尝试删除一个字节或把它换成 0x00、'a'、0xFF，
 * 只要 fails 仍报告失败就保留修改。返回缩小后的长度。
 */
typedef int (*diff_fail_fn)(const unsigned char *data, size_t len, void *ctx);

size_t diff_shrink(unsigned char *data, size_t len, diff_fail_fn fails,
                   void *ctx);

#endif /* DIFF_UTIL_H */