#include <stdint.h>
#include <stdio.h>

/*
 * 引脚编号编码：高 4 位为端口号 (0=PA, 1=PB, 2=PC)，低 4 位为引脚号 (0..15)
 * 例如 PA9 = 0x09, PB6 = 0x16, PC13 = 0x2D
 */
#define PIN_ID(port_char, pin_num) \
    ((uint8_t)((((uint8_t)((port_char) - 'A')) << 4u) | ((uint8_t)(pin_num) & 0x0Fu)))

#define PIN_NONE 0xFFu

typedef struct {
    const char *periph_name;
    uint8_t pins[4];
} periph_pinout_t;

/*
 * 根据 AFIO_MAPR 寄存器的值，填入各外设实际占用的引脚。
 * AFIO_MAPR 相关位：
 *   位 0     SPI1_REMAP   : 0 -> PA4/PA5/PA6/PA7,  1 -> PA15/PB3/PB4/PB5
 *   位 1     I2C1_REMAP   : 0 -> PB6/PB7,          1 -> PB8/PB9
 *   位 2     USART1_REMAP : 0 -> PA9/PA10,         1 -> PB6/PB7
 *   位 14:13 CAN_REMAP    : 00 -> PA11/PA12,       10 -> PB8/PB9
 */
static void resolve_pinout(uint32_t afio_mapr, periph_pinout_t out[4]) {
    uint32_t spi1_remap = (afio_mapr >> 0u) & 0x1u;
    uint32_t i2c1_remap = (afio_mapr >> 1u) & 0x1u;
    uint32_t usart1_remap = (afio_mapr >> 2u) & 0x1u;
    uint32_t can_remap = (afio_mapr >> 13u) & 0x3u;

    out[0].periph_name = "SPI1";
    if (spi1_remap == 0u) {
        out[0].pins[0] = PIN_ID('A', 4);
        out[0].pins[1] = PIN_ID('A', 5);
        out[0].pins[2] = PIN_ID('A', 6);
        out[0].pins[3] = PIN_ID('A', 7);
    } else {
        out[0].pins[0] = PIN_ID('A', 15);
        out[0].pins[1] = PIN_ID('B', 3);
        out[0].pins[2] = PIN_ID('B', 4);
        out[0].pins[3] = PIN_ID('B', 5);
    }

    out[1].periph_name = "I2C1";
    if (i2c1_remap == 0u) {
        out[1].pins[0] = PIN_ID('B', 6);
        out[1].pins[1] = PIN_ID('B', 7);
    } else {
        out[1].pins[0] = PIN_ID('B', 8);
        out[1].pins[1] = PIN_ID('B', 9);
    }
    out[1].pins[2] = PIN_NONE;
    out[1].pins[3] = PIN_NONE;

    out[2].periph_name = "USART1";
    if (usart1_remap == 0u) {
        out[2].pins[0] = PIN_ID('A', 9);
        out[2].pins[1] = PIN_ID('A', 10);
    } else {
        out[2].pins[0] = PIN_ID('B', 6);
        out[2].pins[1] = PIN_ID('B', 7);
    }
    out[2].pins[2] = PIN_NONE;
    out[2].pins[3] = PIN_NONE;

    out[3].periph_name = "CAN1";
    if (can_remap == 2u) {
        out[3].pins[0] = PIN_ID('B', 8);
        out[3].pins[1] = PIN_ID('B', 9);
    } else {
        out[3].pins[0] = PIN_ID('A', 11);
        out[3].pins[1] = PIN_ID('A', 12);
    }
    out[3].pins[2] = PIN_NONE;
    out[3].pins[3] = PIN_NONE;
}

/*
 * TODO: 实现冲突检查函数 check_pin_conflicts
 * 遍历 out[0..count-1] 中所有有效引脚（不等于 PIN_NONE），
 * 若发现两个不同外设占用了同一个引脚，按如下格式打印并返回冲突总数：
 *   CONFLICT: <外设A> and <外设B> on P<端口><引脚号>
 */
static int check_pin_conflicts(const periph_pinout_t *out, size_t count) {
    (void)out;
    (void)count;
    return 0;
}

int main(void) {
    periph_pinout_t table[4];

    printf("--- Case 1: AFIO_MAPR = 0x00000004 (USART1_REMAP=1) ---\n");
    resolve_pinout(0x00000004u, table);
    printf("Conflicts: %d\n", check_pin_conflicts(table, 4));

    printf("--- Case 2: AFIO_MAPR = 0x00004000 (CAN_REMAP=10) ---\n");
    resolve_pinout(0x00004000u, table);
    printf("Conflicts: %d\n", check_pin_conflicts(table, 4));

    return 0;
}
