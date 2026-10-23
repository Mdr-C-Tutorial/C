/*
 * 习题 60393 初始代码：模拟块设备在第 N 次编程时掉电，验证 littlefs 原子更新
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint32_t prog_calls;
    uint32_t fail_at_call; /* 0 表示不注入故障；>0 表示在第 fail_at_call 次 prog 时只写前一半并返回错误 */
} powerloss_bd_ctx_t;

int main(void) {
    powerloss_bd_ctx_t ctx = {.prog_calls = 0u, .fail_at_call = 12u};
    printf("powerloss test context ready (fail_at_call=%u)\n",
           (unsigned)ctx.fail_at_call);
    return 0;
}
