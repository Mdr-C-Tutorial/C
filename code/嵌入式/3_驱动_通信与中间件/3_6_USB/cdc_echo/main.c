#include "clock.h"
#include "stm32f1xx.h"
#include "tusb.h"
#include <stdint.h>

/*
 * Blue Pill 的 D+（PA12）上焊有固定 1.5 kΩ 上拉电阻。下载新程序或复位时，
 * 主机看不到 D+ 掉电，可能不重新枚举。启动 USB 外设前先把 PA12 短暂拉低数毫秒，
 * 模拟一次拔插，促使主机重新发起总线复位与枚举。
 */
static void usb_reenumerate_pulse(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    /* PA12（CRH[19:16]）设为 2 MHz 通用推挽输出（0x2），输出低电平 */
    GPIOA->CRH = (GPIOA->CRH & ~(0xFu << 16)) | (0x2u << 16);
    GPIOA->BSRR = GPIO_BSRR_BR12;
    for (volatile uint32_t i = 0u; i < 72000u; ++i) {
    }
    /* 恢复 PA12 为浮空输入（0x4），随后由 USB 外设接管引脚 */
    GPIOA->CRH = (GPIOA->CRH & ~(0xFu << 16)) | (0x4u << 16);
}

static void led_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    /* PC13（CRH[23:20]）设为 2 MHz 通用推挽输出（0x2），初始置高（熄灭） */
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x2u << 20);
    GPIOC->BSRR = GPIO_BSRR_BS13;
}

static void led_toggle(void) {
    if ((GPIOC->ODR & GPIO_ODR_ODR13) != 0u) {
        GPIOC->BSRR = GPIO_BSRR_BR13;
    } else {
        GPIOC->BSRR = GPIO_BSRR_BS13;
    }
}

/* STM32F103 的三个 USB 中断入口全部转交给 TinyUSB 设备中断处理函数 */
void USB_HP_CAN1_TX_IRQHandler(void) {
    tud_int_handler(0);
}

void USB_LP_CAN1_RX0_IRQHandler(void) {
    tud_int_handler(0);
}

void USBWakeUp_IRQHandler(void) {
    tud_int_handler(0);
}

static void cdc_echo_task(void) {
    if (!tud_cdc_available()) {
        return;
    }

    uint8_t buf[64];
    uint32_t count = tud_cdc_read(buf, sizeof buf);
    if (count > 0u) {
        tud_cdc_write(buf, count);
        tud_cdc_write_flush();
        led_toggle();
    }
}

int main(void) {
    /* HSE 8 MHz × 9 = 72 MHz，USBPRE = 0（PLL ÷ 1.5 = 48 MHz） */
    clock_init_72mhz();
    led_init();
    usb_reenumerate_pulse();

    /* 开启 APB1 上的 USB 外设时钟，初始化 TinyUSB 设备协议栈 */
    RCC->APB1ENR |= RCC_APB1ENR_USBEN;
    tusb_init();

    for (;;) {
        tud_task();
        cdc_echo_task();
    }
}
