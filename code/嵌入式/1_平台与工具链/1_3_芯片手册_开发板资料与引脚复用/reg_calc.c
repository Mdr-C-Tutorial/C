#include <stdint.h>
#include <stdio.h>

#define APB2PERIPH_BASE 0x40010000u
#define GPIOA_OFFSET    0x00000800u
#define GPIO_PORT_STRIDE 0x00000400u
#define GPIO_CRL_OFFSET 0x00u
#define GPIO_CRH_OFFSET 0x04u
#define GPIO_CR_RESET   0x44444444u

typedef struct {
    char port;
    uint32_t pin;
    uint32_t cnf;
    uint32_t mode;
    const char *desc;
} gpio_cfg_t;

static void print_gpio_config(const gpio_cfg_t *cfg) {
    uint32_t port_idx = (uint32_t)(cfg->port - 'A');
    uint32_t port_base = APB2PERIPH_BASE + GPIOA_OFFSET + port_idx * GPIO_PORT_STRIDE;
    int is_high = (cfg->pin >= 8u);
    uint32_t reg_offset = is_high ? GPIO_CRH_OFFSET : GPIO_CRL_OFFSET;
    uint32_t reg_addr = port_base + reg_offset;
    uint32_t bit_lsb = (cfg->pin % 8u) * 4u;
    uint32_t bit_msb = bit_lsb + 3u;
    uint32_t field_val = ((cfg->cnf & 0x3u) << 2u) | (cfg->mode & 0x3u);
    uint32_t clear_mask = ~(0xFu << bit_lsb);
    uint32_t set_mask = field_val << bit_lsb;
    uint32_t reg_val = (GPIO_CR_RESET & clear_mask) | set_mask;

    printf("引脚 P%c%u（%s）\n", cfg->port, cfg->pin, cfg->desc);
    printf("  寄存器    : GPIO%c_%s (基地址 0x%08X + 偏移 0x%02X = 0x%08X)\n",
           cfg->port, is_high ? "CRH" : "CRL", port_base, reg_offset, reg_addr);
    printf("  位域范围  : [%u:%u] (CNF%u=%u, MODE%u=%u, 组合值 0x%X)\n",
           bit_msb, bit_lsb, cfg->pin, cfg->cnf, cfg->pin, cfg->mode, field_val);
    printf("  清零掩码  : 0x%08X  ~(0xFu << %u)\n", clear_mask, bit_lsb);
    printf("  置位掩码  : 0x%08X   (0x%Xu << %u)\n", set_mask, field_val, bit_lsb);
    printf("  修改后值  : 0x%08X  (复位值 0x%08X)\n\n", reg_val, GPIO_CR_RESET);
}

/* 模拟 rc_w1（读以及写 1 清零）状态寄存器的硬件行为 */
static uint32_t sim_rc_w1_write(uint32_t current_sr, uint32_t bus_write_val) {
    return current_sr & ~bus_write_val;
}

int main(void) {
    gpio_cfg_t items[] = {
        { 'C', 13u, 0x0u, 0x2u, "通用推挽输出, 最大速度 2 MHz" },
        { 'A',  9u, 0x2u, 0x3u, "复用推挽输出, 最大速度 50 MHz" },
    };

    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        print_gpio_config(&items[i]);
    }

    /* 演示 rc_w1 寄存器：假设位 0 和位 1 两个标志同时处于置位状态 (0x03) */
    uint32_t sr_init = 0x03u;
    /* 错误做法：想用 |= (1u << 0) 只清零位 0，读-改-写会把 0x03 | 0x01 = 0x03 写回 */
    uint32_t sr_after_rmw = sim_rc_w1_write(sr_init, sr_init | (1u << 0));
    /* 正确做法：直接写 (1u << 0)，只向位 0 写入 1，其余位写 0 不影响 */
    uint32_t sr_after_direct = sim_rc_w1_write(sr_init, (1u << 0));

    printf("rc_w1 状态寄存器初值        : 0x%02X (位 1 与位 0 均置位)\n", sr_init);
    printf("执行 SR |= (1u << 0) 后     : 0x%02X (位 1 被连带清零!)\n", sr_after_rmw);
    printf("执行 SR  = (1u << 0) 后     : 0x%02X (仅清零位 0，位 1 保留)\n", sr_after_direct);

    return 0;
}
