#include "check.h"
#include "mdr_bits.h"

/* 21.4：位计数、位宽、字节序、带检查的算术与对齐。 */

static void test_popcount_u32(void) {
    CHECK_EQ_SIZE(mdr_popcount_u32(0u), 0u);
    CHECK_EQ_SIZE(mdr_popcount_u32(1u), 1u);
    CHECK_EQ_SIZE(mdr_popcount_u32(0x80000000u), 1u);
    CHECK_EQ_SIZE(mdr_popcount_u32(0xF0F00F0Fu), 16u);
    CHECK_EQ_SIZE(mdr_popcount_u32(UINT32_MAX), 32u);
}

static void test_bswap32(void) {
    CHECK(mdr_bswap32(0u) == 0u);
    CHECK(mdr_bswap32(0x12345678u) == 0x78563412u);
    CHECK(mdr_bswap32(0x01020304u) == 0x04030201u);
    CHECK(mdr_bswap32(mdr_bswap32(0xDEADBEEFu)) == 0xDEADBEEFu);
}

static void test_bswap64(void) {
    CHECK(mdr_bswap64(0u) == 0u);
    CHECK(mdr_bswap64(UINT64_C(0x0123456789ABCDEF)) ==
          UINT64_C(0xEFCDAB8967452301));
    CHECK(mdr_bswap64(mdr_bswap64(UINT64_C(0xDEADBEEFCAFEF00D))) ==
          UINT64_C(0xDEADBEEFCAFEF00D));
}

static void test_clz_u32(void) {
    CHECK_EQ_SIZE(mdr_clz_u32(0u), 32u);
    CHECK_EQ_SIZE(mdr_clz_u32(1u), 31u);
    CHECK_EQ_SIZE(mdr_clz_u32(0x0000FFFFu), 16u);
    CHECK_EQ_SIZE(mdr_clz_u32(0x00010000u), 15u);
    CHECK_EQ_SIZE(mdr_clz_u32(0x80000000u), 0u);
    CHECK_EQ_SIZE(mdr_clz_u32(UINT32_MAX), 0u);
}

static void test_ctz_u32(void) {
    CHECK_EQ_SIZE(mdr_ctz_u32(0u), 32u);
    CHECK_EQ_SIZE(mdr_ctz_u32(1u), 0u);
    CHECK_EQ_SIZE(mdr_ctz_u32(0x80000000u), 31u);
    CHECK_EQ_SIZE(mdr_ctz_u32(0xFFFF0000u), 16u);
    CHECK_EQ_SIZE(mdr_ctz_u32(0x00010000u), 16u);
}

static void test_clz_ctz_u64(void) {
    CHECK_EQ_SIZE(mdr_clz_u64(0u), 64u);
    CHECK_EQ_SIZE(mdr_clz_u64(1u), 63u);
    CHECK_EQ_SIZE(mdr_clz_u64(UINT64_C(0x8000000000000000)), 0u);
    CHECK_EQ_SIZE(mdr_ctz_u64(0u), 64u);
    CHECK_EQ_SIZE(mdr_ctz_u64(UINT64_C(0x8000000000000000)), 63u);
    CHECK_EQ_SIZE(mdr_ctz_u64(UINT64_C(0x0000000100000000)), 32u);
}

static void test_popcount_u64_parity(void) {
    CHECK_EQ_SIZE(mdr_popcount_u64(0u), 0u);
    CHECK_EQ_SIZE(mdr_popcount_u64(UINT64_MAX), 64u);
    CHECK_EQ_SIZE(mdr_popcount_u64(UINT64_C(0xF0F0F0F0F0F0F0F0)), 32u);
    CHECK_EQ_SIZE(mdr_parity_u32(0u), 0u);
    CHECK_EQ_SIZE(mdr_parity_u32(1u), 1u);
    CHECK_EQ_SIZE(mdr_parity_u32(3u), 0u);
    CHECK_EQ_SIZE(mdr_parity_u32(UINT32_MAX), 0u);
}

static void test_power_of_two(void) {
    CHECK(!mdr_has_single_bit_u32(0u));
    CHECK(mdr_has_single_bit_u32(1u));
    CHECK(mdr_has_single_bit_u32(0x80000000u));
    CHECK(!mdr_has_single_bit_u32(0x80000001u));

    CHECK_EQ_SIZE(mdr_bit_width_u32(0u), 0u);
    CHECK_EQ_SIZE(mdr_bit_width_u32(1u), 1u);
    CHECK_EQ_SIZE(mdr_bit_width_u32(0xFFu), 8u);
    CHECK_EQ_SIZE(mdr_bit_width_u32(0x80000000u), 32u);

    CHECK(mdr_bit_floor_u32(0u) == 0u);
    CHECK(mdr_bit_floor_u32(1u) == 1u);
    CHECK(mdr_bit_floor_u32(0xFFu) == 0x80u);
    CHECK(mdr_bit_floor_u32(0x80000000u) == 0x80000000u);

    CHECK(mdr_bit_ceil_u32(0u) == 1u);
    CHECK(mdr_bit_ceil_u32(1u) == 1u);
    CHECK(mdr_bit_ceil_u32(2u) == 2u);
    CHECK(mdr_bit_ceil_u32(3u) == 4u);
    CHECK(mdr_bit_ceil_u32(0x80000000u) == 0x80000000u);
    CHECK(mdr_bit_ceil_u32(0x80000001u) == 0u);
}

static void test_rotate(void) {
    CHECK(mdr_rotl_u32(0x12345678u, 0u) == 0x12345678u);
    CHECK(mdr_rotl_u32(0x12345678u, 8u) == 0x34567812u);
    CHECK(mdr_rotl_u32(0x80000000u, 1u) == 1u);
    CHECK(mdr_rotl_u32(0x12345678u, 32u) == 0x12345678u);
    CHECK(mdr_rotl_u32(0x12345678u, 36u) == 0x23456781u);
    CHECK(mdr_rotr_u32(0x12345678u, 8u) == 0x78123456u);
    CHECK(mdr_rotr_u32(1u, 1u) == 0x80000000u);
    CHECK(mdr_rotl_u32(mdr_rotr_u32(0xDEADBEEFu, 13u), 13u) == 0xDEADBEEFu);
}

static void test_endian(void) {
    unsigned char buf[4];
    uint32_t x = 0x12345678u;

    mdr_store_le32(buf, x);
    CHECK_EQ_SIZE(buf[0], 0x78u);
    CHECK_EQ_SIZE(buf[1], 0x56u);
    CHECK_EQ_SIZE(buf[2], 0x34u);
    CHECK_EQ_SIZE(buf[3], 0x12u);
    CHECK(mdr_load_le32(buf) == x);

    mdr_store_be32(buf, x);
    CHECK_EQ_SIZE(buf[0], 0x12u);
    CHECK_EQ_SIZE(buf[1], 0x34u);
    CHECK_EQ_SIZE(buf[2], 0x56u);
    CHECK_EQ_SIZE(buf[3], 0x78u);
    CHECK(mdr_load_be32(buf) == x);
    CHECK(mdr_load_le32(buf) == 0x78563412u);
}

static void test_checked_arithmetic(void) {
    size_t r = 0;

    CHECK(!mdr_ckd_add_size(2u, 3u, &r));
    CHECK_EQ_SIZE(r, 5u);
    CHECK(!mdr_ckd_add_size(SIZE_MAX, 0u, &r));
    CHECK_EQ_SIZE(r, SIZE_MAX);
    CHECK(mdr_ckd_add_size(SIZE_MAX, 1u, &r));
    CHECK(mdr_ckd_add_size(SIZE_MAX - 1u, 2u, &r));

    CHECK(!mdr_ckd_mul_size(0u, SIZE_MAX, &r));
    CHECK_EQ_SIZE(r, 0u);
    CHECK(!mdr_ckd_mul_size(1u, SIZE_MAX, &r));
    CHECK_EQ_SIZE(r, SIZE_MAX);
    CHECK(!mdr_ckd_mul_size(SIZE_MAX, 1u, &r));
    CHECK(mdr_ckd_mul_size(SIZE_MAX, 2u, &r));
    CHECK(mdr_ckd_mul_size(SIZE_MAX / 2u + 1u, 2u, &r));

    CHECK_EQ_SIZE(mdr_sat_add_u32(1u, 2u), 3u);
    CHECK(mdr_sat_add_u32(UINT32_MAX, 1u) == UINT32_MAX);
    CHECK(mdr_sat_add_u32(0x80000000u, 0x80000000u) == UINT32_MAX);
}

static void test_align_div(void) {
    CHECK_EQ_SIZE(mdr_align_up_size(0u, 8u), 0u);
    CHECK_EQ_SIZE(mdr_align_up_size(1u, 8u), 8u);
    CHECK_EQ_SIZE(mdr_align_up_size(8u, 8u), 8u);
    CHECK_EQ_SIZE(mdr_align_up_size(9u, 8u), 16u);
    CHECK_EQ_SIZE(mdr_align_up_size(13u, 8u), 16u);

    CHECK_EQ_SIZE(mdr_div_ceil_size(0u, 4u), 0u);
    CHECK_EQ_SIZE(mdr_div_ceil_size(1u, 4u), 1u);
    CHECK_EQ_SIZE(mdr_div_ceil_size(4u, 4u), 1u);
    CHECK_EQ_SIZE(mdr_div_ceil_size(5u, 4u), 2u);
    CHECK_EQ_SIZE(mdr_div_ceil_size(SIZE_MAX, 2u), SIZE_MAX / 2u + 1u);
}

int main(void) {
    test_popcount_u32();
    test_bswap32();
    test_bswap64();
    test_clz_u32();
    test_ctz_u32();
    test_clz_ctz_u64();
    test_popcount_u64_parity();
    test_power_of_two();
    test_rotate();
    test_endian();
    test_checked_arithmetic();
    test_align_div();
    return check_report("test_bits");
}