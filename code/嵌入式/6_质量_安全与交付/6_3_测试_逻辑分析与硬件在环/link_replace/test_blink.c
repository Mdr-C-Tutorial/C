/*
 * test_blink.c：用链接替换的 tick_now 测试闪烁逻辑。
 *
 * 讲解见教程 6.3 节「测试、逻辑分析与硬件在环」。
 */
#include <stdio.h>

#include "../common/test_assert.h"
#include "app_logic.h"
#include "tick.h"

void fake_tick_set(uint32_t ms);

void setUp(void) {
    fake_tick_set(0u);
}

void tearDown(void) {
}

static void test_initial_state(void) {
    struct blink_ctx ctx;
    blink_init(&ctx, tick_now());
    TEST_ASSERT_FALSE(ctx.led_on);
    TEST_ASSERT_EQUAL_INT(500, ctx.period_ms);
}

static void test_toggles_at_500ms(void) {
    struct blink_ctx ctx;
    blink_init(&ctx, tick_now());

    fake_tick_set(499u);
    blink_step(&ctx, tick_now());
    TEST_ASSERT_FALSE(ctx.led_on); /* 还没到 */

    fake_tick_set(500u);
    blink_step(&ctx, tick_now());
    TEST_ASSERT_TRUE(ctx.led_on);

    fake_tick_set(999u);
    blink_step(&ctx, tick_now());
    TEST_ASSERT_TRUE(ctx.led_on); /* 差 1 ms，不翻转 */

    fake_tick_set(1000u);
    blink_step(&ctx, tick_now());
    TEST_ASSERT_FALSE(ctx.led_on);
}

static void test_tick_counter_wrap(void) {
    struct blink_ctx ctx;
    /* 时间戳接近 32 位回绕，差值计算仍然正确 */
    uint32_t start = 0xFFFFFF00u;
    fake_tick_set(start);
    blink_init(&ctx, tick_now());

    fake_tick_set(start + 500u); /* 回绕到 0x000000EC */
    blink_step(&ctx, tick_now());
    TEST_ASSERT_TRUE(ctx.led_on);
    TEST_ASSERT_EQUAL_INT(500, ctx.last_toggle_ms - start);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_state);
    RUN_TEST(test_toggles_at_500ms);
    RUN_TEST(test_tick_counter_wrap);
    return UNITY_END();
}
