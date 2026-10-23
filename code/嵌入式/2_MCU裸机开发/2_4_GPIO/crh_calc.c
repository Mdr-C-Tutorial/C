/*
 * crh_calc.c：演示修改 GPIOx_CRH 单个引脚字段时「先清后置」与整体赋值的区别。
 *
 * 在宿主机上编译运行：
 *   gcc -std=c17 -Wall -Wextra crh_calc.c -o crh_calc
 */
#include <stdint.h>
#include <stdio.h>

int main(void) {
    uint32_t crh_reset = 0x44444444u;
    unsigned shift = (13u - 8u) * 4u; /* PC13 位于 CRH 的第 23:20 位 */
    uint32_t clear_mask = ~(0xFu << shift);
    uint32_t set_mask = 0x2u << shift; /* CNF13 = 00，MODE13 = 10（2 MHz 推挽输出） */

    uint32_t crh_good = (crh_reset & clear_mask) | set_mask;
    uint32_t crh_bad = set_mask;

    printf("CRH 复位值          : 0x%08X\n", crh_reset);
    printf("清零掩码 ~(0xFu<<20): 0x%08X\n", clear_mask);
    printf("置位掩码  (0x2u<<20): 0x%08X\n", set_mask);
    printf("先清后置后的 CRH    : 0x%08X (其余引脚保持 0x4 浮空输入)\n", crh_good);
    printf("整体赋值后的 CRH    : 0x%08X (其余引脚全部变成 0x0 模拟输入)\n", crh_bad);
    return 0;
}
