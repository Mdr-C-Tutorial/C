#ifndef MDR_BITS_H
#define MDR_BITS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * 返回 32 位无符号整数 value 中值为 1 的位数。
 * 返回：0 到 32 之间的值。
 */
unsigned mdr_popcount_u32(uint32_t value);

/*
 * 返回 32 位无符号整数 value 按字节反转次序后的结果。
 * 返回：例如 0x12345678 得到 0x78563412。
 */
uint32_t mdr_bswap32(uint32_t value);

/* 21.4：位计数、位宽与字节序 */

/* 前导零、尾随零的个数；输入为 0 时返回位宽（32 或 64），与 C23 <stdbit.h> 一致。 */
unsigned mdr_clz_u32(uint32_t x);
unsigned mdr_clz_u64(uint64_t x);
unsigned mdr_ctz_u32(uint32_t x);
unsigned mdr_ctz_u64(uint64_t x);

/* 64 位置位计数；32 位奇偶校验（1 的个数为奇数时返回 1）。 */
unsigned mdr_popcount_u64(uint64_t x);
unsigned mdr_parity_u32(uint32_t x);

/* 是否为 2 的幂（0 不是）。 */
bool mdr_has_single_bit_u32(uint32_t x);

/* 位宽，即表示 x 所需的最少位数；x 为 0 时返回 0。 */
unsigned mdr_bit_width_u32(uint32_t x);

/* 不超过 x 的最大 2 的幂；x 为 0 时返回 0。 */
uint32_t mdr_bit_floor_u32(uint32_t x);

/* 不小于 x 的最小 2 的幂；结果超出 uint32_t 时返回 0。
 * C23 <stdbit.h> 的 stdc_bit_ceil 对放不下的输入返回 0。 */
uint32_t mdr_bit_ceil_u32(uint32_t x);

/* 循环移位；移位量按 k % 32 处理。 */
uint32_t mdr_rotl_u32(uint32_t x, unsigned k);
uint32_t mdr_rotr_u32(uint32_t x, unsigned k);

/* 64 位字节次序反转。 */
uint64_t mdr_bswap64(uint64_t x);

/* 与主机字节序无关地从缓冲区读写 32 位整数。 */
uint32_t mdr_load_le32(const void *src);
void mdr_store_le32(void *dst, uint32_t x);
uint32_t mdr_load_be32(const void *src);
void mdr_store_be32(void *dst, uint32_t x);

/* 带溢出检查的 size_t 加法与乘法：结果写入 *res，溢出返回 true。 */
bool mdr_ckd_add_size(size_t a, size_t b, size_t *res);
bool mdr_ckd_mul_size(size_t a, size_t b, size_t *res);

/* 饱和加法：结果超过 UINT32_MAX 时返回 UINT32_MAX。 */
uint32_t mdr_sat_add_u32(uint32_t a, uint32_t b);

/* 把 x 向上对齐到 align 的倍数；调用方保证 align 是 2 的幂且结果不溢出。 */
size_t mdr_align_up_size(size_t x, size_t align);

/* 向上取整除法；调用方保证 b 不为 0。 */
size_t mdr_div_ceil_size(size_t a, size_t b);

#endif /* MDR_BITS_H */