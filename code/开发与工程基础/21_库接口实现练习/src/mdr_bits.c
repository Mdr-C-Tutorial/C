#include "mdr_bits.h"
#include "mdr_compiler.h"

/*
 * 21.4 用普通 C 实现的位与整数辅助函数。
 * 21.6 在不定义 MDR_NO_BUILTINS 时优先走编译器内建函数，下面的普通 C 版本
 * 保留为回退分支；两条路径对所有输入的结果相同。
 */

unsigned mdr_popcount_u32(uint32_t value) {
#if MDR_HAVE_STDBIT
    return (unsigned)stdc_count_ones_ui(value);
#elif MDR_USE_BUILTINS && MDR_HAS_BUILTIN(__builtin_popcount)
    return (unsigned)__builtin_popcount(value);
#elif MDR_USE_BUILTINS && defined(_MSC_VER)
    return (unsigned)__popcnt(value);
#else
    unsigned count = 0;

    while (value != 0) {
        value &= value - 1u;
        count++;
    }
    return count;
#endif
}

unsigned mdr_clz_u32(uint32_t x) {
#if MDR_HAVE_STDBIT
    return (unsigned)stdc_leading_zeros_ui(x);
#elif MDR_USE_BUILTINS && MDR_HAS_BUILTIN(__builtin_clz)
    return x == 0 ? 32u : (unsigned)__builtin_clz(x);
#elif MDR_USE_BUILTINS && defined(_MSC_VER)
    unsigned long index;

    return _BitScanReverse(&index, x) ? 31u - (unsigned)index : 32u;
#else
    unsigned n = 0;

    if (x == 0) {
        return 32;
    }
    while ((x & 0x80000000u) == 0) {
        x <<= 1;
        n++;
    }
    return n;
#endif
}

unsigned mdr_ctz_u32(uint32_t x) {
#if MDR_HAVE_STDBIT
    return (unsigned)stdc_trailing_zeros_ui(x);
#elif MDR_USE_BUILTINS && MDR_HAS_BUILTIN(__builtin_ctz)
    return x == 0 ? 32u : (unsigned)__builtin_ctz(x);
#elif MDR_USE_BUILTINS && defined(_MSC_VER)
    unsigned long index;

    return _BitScanForward(&index, x) ? (unsigned)index : 32u;
#else
    unsigned n = 0;

    if (x == 0) {
        return 32;
    }
    while ((x & 1u) == 0) {
        x >>= 1;
        n++;
    }
    return n;
#endif
}

unsigned mdr_clz_u64(uint64_t x) {
#if MDR_HAVE_STDBIT
    return (unsigned)stdc_leading_zeros_ull(x);
#elif MDR_USE_BUILTINS && MDR_HAS_BUILTIN(__builtin_clzll)
    return x == 0 ? 64u : (unsigned)__builtin_clzll(x);
#elif MDR_USE_BUILTINS && defined(_MSC_VER) && defined(_M_X64)
    unsigned long index;

    return _BitScanReverse64(&index, x) ? 63u - (unsigned)index : 64u;
#else
    unsigned n = 0;

    if (x == 0) {
        return 64;
    }
    while ((x & UINT64_C(0x8000000000000000)) == 0) {
        x <<= 1;
        n++;
    }
    return n;
#endif
}

unsigned mdr_ctz_u64(uint64_t x) {
#if MDR_HAVE_STDBIT
    return (unsigned)stdc_trailing_zeros_ull(x);
#elif MDR_USE_BUILTINS && MDR_HAS_BUILTIN(__builtin_ctzll)
    return x == 0 ? 64u : (unsigned)__builtin_ctzll(x);
#elif MDR_USE_BUILTINS && defined(_MSC_VER) && defined(_M_X64)
    unsigned long index;

    return _BitScanForward64(&index, x) ? (unsigned)index : 64u;
#else
    unsigned n = 0;

    if (x == 0) {
        return 64;
    }
    while ((x & 1u) == 0) {
        x >>= 1;
        n++;
    }
    return n;
#endif
}

unsigned mdr_popcount_u64(uint64_t x) {
#if MDR_HAVE_STDBIT
    return (unsigned)stdc_count_ones_ull(x);
#elif MDR_USE_BUILTINS && MDR_HAS_BUILTIN(__builtin_popcountll)
    return (unsigned)__builtin_popcountll(x);
#elif MDR_USE_BUILTINS && defined(_MSC_VER) && defined(_M_X64)
    return (unsigned)__popcnt64(x);
#else
    unsigned count = 0;

    while (x != 0) {
        x &= x - 1;
        count++;
    }
    return count;
#endif
}

unsigned mdr_parity_u32(uint32_t x) {
    x ^= x >> 16;
    x ^= x >> 8;
    x ^= x >> 4;
    x ^= x >> 2;
    x ^= x >> 1;
    return x & 1u;
}

bool mdr_has_single_bit_u32(uint32_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

unsigned mdr_bit_width_u32(uint32_t x) {
    return 32 - mdr_clz_u32(x);
}

uint32_t mdr_bit_floor_u32(uint32_t x) {
    if (x == 0) {
        return 0;
    }
    return 1u << (31 - mdr_clz_u32(x));
}

uint32_t mdr_bit_ceil_u32(uint32_t x) {
    if (x <= 1) {
        return 1;
    }
    if (x > 0x80000000u) {
        return 0;
    }
    return 1u << (32 - mdr_clz_u32(x - 1));
}

uint32_t mdr_rotl_u32(uint32_t x, unsigned k) {
    k &= 31;
    return (x << k) | (x >> ((32 - k) & 31));
}

uint32_t mdr_rotr_u32(uint32_t x, unsigned k) {
    k &= 31;
    return (x >> k) | (x << ((32 - k) & 31));
}

uint32_t mdr_bswap32(uint32_t value) {
    return ((value & 0x000000FFu) << 24) | ((value & 0x0000FF00u) << 8) |
           ((value & 0x00FF0000u) >> 8) | ((value & 0xFF000000u) >> 24);
}

uint64_t mdr_bswap64(uint64_t x) {
    return ((uint64_t)mdr_bswap32((uint32_t)x) << 32) |
           mdr_bswap32((uint32_t)(x >> 32));
}

uint32_t mdr_load_le32(const void *src) {
    const unsigned char *p = (const unsigned char *)src;

    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

void mdr_store_le32(void *dst, uint32_t x) {
    unsigned char *p = (unsigned char *)dst;

    p[0] = (unsigned char)(x & 0xFFu);
    p[1] = (unsigned char)((x >> 8) & 0xFFu);
    p[2] = (unsigned char)((x >> 16) & 0xFFu);
    p[3] = (unsigned char)((x >> 24) & 0xFFu);
}

uint32_t mdr_load_be32(const void *src) {
    const unsigned char *p = (const unsigned char *)src;

    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

void mdr_store_be32(void *dst, uint32_t x) {
    unsigned char *p = (unsigned char *)dst;

    p[0] = (unsigned char)((x >> 24) & 0xFFu);
    p[1] = (unsigned char)((x >> 16) & 0xFFu);
    p[2] = (unsigned char)((x >> 8) & 0xFFu);
    p[3] = (unsigned char)(x & 0xFFu);
}

bool mdr_ckd_add_size(size_t a, size_t b, size_t *res) {
    return MDR_CKD_ADD_SIZE(a, b, res);
}

bool mdr_ckd_mul_size(size_t a, size_t b, size_t *res) {
    if (b != 0 && a > SIZE_MAX / b) {
        return true;
    }
    *res = a * b;
    return false;
}

uint32_t mdr_sat_add_u32(uint32_t a, uint32_t b) {
    uint64_t sum = (uint64_t)a + b;

    return sum > UINT32_MAX ? UINT32_MAX : (uint32_t)sum;
}

size_t mdr_align_up_size(size_t x, size_t align) {
    return (x + align - 1) & ~(align - 1);
}

size_t mdr_div_ceil_size(size_t a, size_t b) {
    return a / b + (a % b != 0);
}