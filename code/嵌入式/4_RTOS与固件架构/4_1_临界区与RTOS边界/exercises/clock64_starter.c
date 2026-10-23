/*
 * exercises/clock64_starter.c：习题 60414 起始代码。
 *
 * 64 位单调系统时钟由两部分拼成：
 *   - 低 32 位：硬件自由递增计数器 TIM_CNT（0x00000000 -> 0xFFFFFFFF 后回绕为 0）
 *   - 高 32 位：每次计数器回绕触发更新中断时由 ISR 递增的 g_WrapCount
 *
 * 任务：在不关中断、不阻塞中断的前提下补全 clock64_read()，
 * 使其即便在读取中途遭遇一次计数器溢出中断，也能返回前后一致的 64 位计数值。
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static volatile uint32_t g_wrap_count;
static volatile uint32_t g_sim_tim_cnt;

uint64_t clock64_read(void) {
    /* TODO: 读取 g_wrap_count 与 g_sim_tim_cnt 并检查中途是否发生回绕 */
    uint32_t hi = g_wrap_count;
    uint32_t lo = g_sim_tim_cnt;
    return ((uint64_t)hi << 32) | lo;
}

int main(void) {
    g_wrap_count = 5u;
    g_sim_tim_cnt = 0xFFFFFFFEu;
    uint64_t t1 = clock64_read();

    /* 模拟正常回绕后的状态 */
    g_wrap_count = 6u;
    g_sim_tim_cnt = 0x00000003u;
    uint64_t t2 = clock64_read();

    assert(t2 > t1);
    printf("t1 = 0x%016llX\nt2 = 0x%016llX\n",
           (unsigned long long)t1,
           (unsigned long long)t2);
    return 0;
}
