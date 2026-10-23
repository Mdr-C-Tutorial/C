#include <stdio.h>

// 模拟裸机前后台系统在 20 ms 窗口（t = 0..19 ms）内的调度
// 三个后台任务依次在超级循环中执行：
// 1. key_scan: 耗时 1 ms，若 irq_flag 为 1 则处理按键事件并清零 irq_flag
// 2. temp_calc: 耗时 temp_cost_ms（第一次实验取 4，第二次实验取 8）
// 3. led_update: 耗时 1 ms

void run_simulation(int temp_cost_ms, int irq_trigger_ms) {
    int t = 0;
    int irq_flag = 0;
    int handled_at = -1;

    // TODO: 模拟超级循环按顺序执行 key_scan -> temp_calc -> led_update，
    // 在每个任务的起止时间段内检查是否到达 irq_trigger_ms 并置位 irq_flag，
    // 记录 key_scan 实际检测到 irq_flag 并完成响应的时刻 handled_at。
    (void)t;
    (void)irq_flag;

    printf("temp_cost=%d ms, irq_at=%d ms -> handled_at=%d ms, latency=%d ms\n",
           temp_cost_ms, irq_trigger_ms, handled_at,
           handled_at - irq_trigger_ms);
}

int main(void) {
    run_simulation(4, 2);
    run_simulation(8, 2);
    return 0;
}
