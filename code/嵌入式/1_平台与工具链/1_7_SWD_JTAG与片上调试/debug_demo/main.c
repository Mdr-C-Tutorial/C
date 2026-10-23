/*
 * debug_demo/main.c：片上调试演示程序（仅使用 Cortex-M3 内核与 SRAM）。
 *
 * 构建与调试步骤见教程 1.7 节「SWD、JTAG 与片上调试」。
 */
#include <stdint.h>

volatile uint32_t g_mode = 1;
int32_t g_samples[4] = {10, 20, 30, 40};
volatile uint32_t g_tick_count = 0;
volatile int32_t g_last_sum = 0;

__attribute__((noinline)) int32_t sum_array(const int32_t *arr, uint32_t len) {
    int32_t total = 0;
    for (uint32_t i = 0; i < len; ++i) {
        total += arr[i];
    }
    return total;
}

/*
 * 缺陷演示：g_samples 只有 4 个元素（合法下标 0～3），但这里误按 0～4 取模。
 * 当 idx 为 4 时，指针 p 越界指向紧邻 g_samples 之后的 g_mode 并将其改写。
 */
__attribute__((noinline)) void record_sample(uint32_t step, int32_t val) {
    uint32_t idx = step % 5u;
    int32_t *p = g_samples + idx;
    *p = val;
}

int main(void) {
    for (;;) {
        ++g_tick_count;
        record_sample(g_tick_count, (int32_t)(g_tick_count * 10u));
        if (g_mode == 1u) {
            g_last_sum = sum_array(g_samples, 4);
        } else {
            g_last_sum = -1;
        }
    }
}
