/*
 * cfgr_decode_starter.c：习题 62223 初始代码。
 * 根据 RCC_CFGR 与 FLASH_ACR 寄存器值推算各路时钟并检查是否符合 RM0008 规范。
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint32_t sysclk_hz;
    uint32_t hclk_hz;
    uint32_t pclk1_hz;
    uint32_t tim_apb1_hz;
    uint32_t pclk2_hz;
    uint32_t adcclk_hz;
    bool limits_ok;
    bool flash_ws_ok;
} clock_report_t;

/*
 * TODO：补全 cfgr_inspect()，根据 cfgr、acr 与外部晶振频率 hse_hz（HSI 固定为 8 MHz）
 * 填写 out 的各个字段：
 * 1. limits_ok：SYSCLK <= 72 MHz，HCLK <= 72 MHz，PCLK1 <= 36 MHz，
 *               PCLK2 <= 72 MHz，ADCCLK <= 14 MHz 时为 true；
 * 2. flash_ws_ok：FLASH_ACR 的 LATENCY[2:0] 不少于 SYSCLK 所需的等待周期数时为 true
 *    （<= 24 MHz 需 >= 0，<= 48 MHz 需 >= 1，<= 72 MHz 需 >= 2）。
 */
void cfgr_inspect(uint32_t cfgr, uint32_t acr, uint32_t hse_hz,
                  clock_report_t *out) {
    (void)cfgr;
    (void)acr;
    (void)hse_hz;
    *out = (clock_report_t){0};
}

int main(void) {
    struct {
        const char *label;
        uint32_t cfgr;
        uint32_t acr;
    } cases[] = {
        {"72 MHz valid",   0x001D840Au, 0x00000032u},
        {"APB1 overclock", 0x001D800Au, 0x00000032u},
        {"WS too small",   0x001D840Au, 0x00000030u},
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        clock_report_t rep;
        cfgr_inspect(cases[i].cfgr, cases[i].acr, 8000000u, &rep);
        printf("%-16s SYSCLK=%u PCLK1=%u TIM2=%u ADC=%u limits=%d ws=%d\n",
               cases[i].label, rep.sysclk_hz, rep.pclk1_hz,
               rep.tim_apb1_hz, rep.adcclk_hz,
               rep.limits_ok ? 1 : 0, rep.flash_ws_ok ? 1 : 0);
    }
    return 0;
}
