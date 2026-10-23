#include "modbus/rtu_slave.h"
#include "stm32f1xx.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTU_RX_BUF_CAP 256u

static volatile uint8_t g_rx_buf[RTU_RX_BUF_CAP];
static volatile size_t g_rx_len = 0u;
static volatile bool g_frame_ready = false;

/*
 * RS-485 方向控制：PB5 接 MAX3485 的 DE 与 /RE
 * 高电平：驱动器使能（发送）
 * 低电平：接收器使能（接收）
 */
static void rs485_dir_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    GPIOB->CRL = (GPIOB->CRL & ~(0xFu << 20)) | (0x2u << 20); /* PB5 2 MHz 推挽输出 */
    GPIOB->BSRR = GPIO_BSRR_BR5; /* 默认处于接收态 */
}

static void rs485_send_bytes(const uint8_t *buf, size_t len) {
    GPIOB->BSRR = GPIO_BSRR_BS5; /* DE = 1，切换为发送态 */

    for (size_t i = 0u; i < len; ++i) {
        while ((USART2->SR & USART_SR_TXE) == 0u) {
        }
        USART2->DR = buf[i];
    }

    /* 必须等待最后一个字节从移位寄存器完整发出（TC = 1），再拉低 DE */
    while ((USART2->SR & USART_SR_TC) == 0u) {
    }
    GPIOB->BSRR = GPIO_BSRR_BR5; /* DE = 0，切回接收态 */
}

/*
 * 配置 TIM2 为单脉冲模式（OPM），用于检测 t3.5 帧间静默（1750 us）：
 * TIM2 时钟 = 72 MHz，PSC = 72 - 1 -> 1 MHz（1 us/tick），ARR = 1750 - 1
 */
static void tim2_t35_init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    TIM2->CR1 = TIM_CR1_OPM | TIM_CR1_URS;
    TIM2->PSC = 72u - 1u;
    TIM2->ARR = 1750u - 1u;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0u;
    TIM2->DIER = TIM_DIER_UIE;

    NVIC_SetPriority(TIM2_IRQn, 2u);
    NVIC_EnableIRQ(TIM2_IRQn);
}

static void usart2_init_115200(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* PA2（TX）：50 MHz 复用推挽（0xB）；PA3（RX）：上拉输入（0x8，ODR3 = 1） */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFFu << 8)) | (0x8Bu << 8);
    GPIOA->BSRR = (1u << 3);

    /* PCLK1 = 36 MHz，115200 波特率：USARTDIV = 36000000 / (16 * 115200) = 19.53125 -> 0x0139 */
    USART2->BRR = 0x0139u;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;

    NVIC_SetPriority(USART2_IRQn, 1u);
    NVIC_EnableIRQ(USART2_IRQn);
}

void USART2_IRQHandler(void) {
    uint32_t sr = USART2->SR;
    if ((sr & (USART_SR_RXNE | USART_SR_ORE)) != 0u) {
        uint8_t byte = (uint8_t)USART2->DR;
        if (!g_frame_ready) {
            if (g_rx_len < RTU_RX_BUF_CAP) {
                g_rx_buf[g_rx_len++] = byte;
            }
            /* 每收到一个字节，将 TIM2 计数器清零并重新启动 1.75 ms 定时 */
            TIM2->CNT = 0u;
            TIM2->SR = 0u;
            TIM2->CR1 |= TIM_CR1_CEN;
        }
    }
}

void TIM2_IRQHandler(void) {
    if ((TIM2->SR & TIM_SR_UIF) != 0u) {
        TIM2->SR = 0u;
        if (g_rx_len > 0u) {
            g_frame_ready = true;
        }
    }
}

int main(void) {
    struct modbus_rtu_slave slave = {
        .slave_addr = 0x01u,
        .holding_regs = { 100u, 200u, 300u, 400u },
    };
    uint8_t req_copy[RTU_RX_BUF_CAP];
    uint8_t resp_buf[256];

    rs485_dir_init();
    tim2_t35_init();
    usart2_init_115200();

    for (;;) {
        if (g_frame_ready) {
            size_t len = g_rx_len;
            for (size_t i = 0u; i < len; ++i) {
                req_copy[i] = g_rx_buf[i];
            }
            g_rx_len = 0u;
            g_frame_ready = false;

            size_t resp_len = modbus_rtu_process(&slave, req_copy, len,
                                                 resp_buf, sizeof resp_buf);
            if (resp_len > 0u) {
                rs485_send_bytes(resp_buf, resp_len);
            }
        }
    }
}
