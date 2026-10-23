/*
 * stm32_board.c：目标板（Blue Pill）上的板级实现。
 *
 * 与 sim_main.c 一一对应，换掉的只是底座：
 *   - 传感器：I²C1 的 PB6/PB7 上用 GPIO 模拟时序读 MPU6050（7.1 的总线层）；
 *   - 日志：W25Q64 的 0x030000 起 256 KB 分区，SPI1 + PA4 片选；
 *   - 控制台：USART1 接收中断写进 FreeRTOS 流缓冲区，命令任务从这里取行；
 *   - 看门狗：IWDG，约 2 秒超时，由监视任务喂。
 *
 * 本文件只做编译与链接检查；板上现象无法在这里验证。
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "task.h"

#include "board.h"
#include "lfs.h"
#include "recorder.h"

#include "stm32f1xx.h"

/* ---------- 时钟 ---------- */

uint32_t SystemCoreClock = 8000000u;

static void clock_init_72mhz(void) {
    RCC->CR |= RCC_CR_HSEON;
    while ((RCC->CR & RCC_CR_HSERDY) == 0) {
    }

    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_2 | FLASH_ACR_PRFTBE;
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 |
                               RCC_CFGR_ADCPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL)) |
                RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE2_DIV1 | RCC_CFGR_PPRE1_DIV2 |
                RCC_CFGR_ADCPRE_DIV6 | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;

    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0) {
    }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
    }

    SystemCoreClock = 72000000u;
}

/* ---------- 控制台：USART1 + 流缓冲区 ---------- */

#define CONSOLE_RX_SIZE 128u

static StreamBufferHandle_t g_rx_stream;

void USART1_IRQHandler(void) {
    BaseType_t woken = pdFALSE;

    if ((USART1->SR & USART_SR_RXNE) != 0u) {
        uint8_t byte = (uint8_t)USART1->DR;
        (void)xStreamBufferSendFromISR(g_rx_stream, &byte, 1u, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

static void console_init_uart(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    /* PA9：50 MHz 复用推挽输出；PA10：浮空输入 */
    GPIOA->CRH = (GPIOA->CRH & ~(0xFFu << 4)) | (0x4Bu << 4);

    USART1->BRR = (72000000u + 115200u / 2u) / 115200u;
    USART1->CR2 = 0;
    USART1->CR3 = 0;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

    /* 数值 6 的左移形式：只有 >= configMAX_SYSCALL_INTERRUPT_PRIORITY(5) 的
     * 中断才能调用 FromISR API（4.4） */
    NVIC_SetPriority(USART1_IRQn, 6u);
    NVIC_EnableIRQ(USART1_IRQn);

    g_rx_stream = xStreamBufferCreate(CONSOLE_RX_SIZE, 1u);
}

static void uart_putc(char c) {
    while ((USART1->SR & USART_SR_TXE) == 0) {
    }
    USART1->DR = (uint8_t)c;
}

int _write(int fd, const char *buf, int len) {
    (void)fd;
    for (int i = 0; i < len; ++i) {
        if (buf[i] == '\n') {
            uart_putc('\r');
        }
        uart_putc(buf[i]);
    }
    return len;
}

void recorder_console_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    taskENTER_CRITICAL();
    vprintf(fmt, args);
    fflush(stdout);
    taskEXIT_CRITICAL();
    va_end(args);
}

bool board_console_read_line(char *buf, size_t max, TickType_t wait) {
    size_t used = 0;

    for (;;) {
        char c;
        if (xStreamBufferReceive(g_rx_stream, &c, 1u, wait) == 0u) {
            return false;
        }
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            buf[used] = '\0';
            return true;
        }
        if (used + 1u < max) {
            buf[used++] = c;
        }
    }
}

void board_exit(void) {
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

/* ---------- I²C 与 MPU6050 ---------- */

static void i2c_delay_half(void) {
    for (volatile uint32_t i = 0; i < 45u; ++i) {
        __NOP();
    }
}

static inline void scl(uint32_t level) {
    GPIOB->BSRR = (level != 0u) ? GPIO_BSRR_BS6 : GPIO_BSRR_BR6;
}

static inline void sda(uint32_t level) {
    GPIOB->BSRR = (level != 0u) ? GPIO_BSRR_BS7 : GPIO_BSRR_BR7;
}

static inline uint32_t sda_read(void) {
    return (GPIOB->IDR & GPIO_IDR_IDR7) != 0u ? 1u : 0u;
}

static void i2c_init_pins(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    GPIOB->BSRR = GPIO_BSRR_BS6 | GPIO_BSRR_BS7;
    /* PB6、PB7：50 MHz 开漏输出（CNF=01, MODE=11 -> 0x7） */
    GPIOB->CRL = (GPIOB->CRL & ~(0xFFu << 24)) | (0x77u << 24);
}

static void i2c_start(void) {
    sda(1u);
    scl(1u);
    i2c_delay_half();
    sda(0u);
    i2c_delay_half();
    scl(0u);
    i2c_delay_half();
}

static void i2c_stop(void) {
    sda(0u);
    i2c_delay_half();
    scl(1u);
    i2c_delay_half();
    sda(1u);
    i2c_delay_half();
}

static bool i2c_write_byte(uint8_t byte) {
    for (uint32_t i = 0; i < 8u; ++i) {
        sda((byte & 0x80u) != 0u ? 1u : 0u);
        byte <<= 1;
        i2c_delay_half();
        scl(1u);
        i2c_delay_half();
        scl(0u);
    }
    sda(1u);
    i2c_delay_half();
    scl(1u);
    i2c_delay_half();
    bool ack = sda_read() == 0u;
    scl(0u);
    i2c_delay_half();
    return ack;
}

static uint8_t i2c_read_byte(bool ack) {
    uint8_t rx = 0u;
    sda(1u);
    for (uint32_t i = 0; i < 8u; ++i) {
        i2c_delay_half();
        scl(1u);
        i2c_delay_half();
        rx = (uint8_t)((rx << 1) | (sda_read() & 1u));
        scl(0u);
    }
    sda(ack ? 0u : 1u);
    i2c_delay_half();
    scl(1u);
    i2c_delay_half();
    scl(0u);
    i2c_delay_half();
    return rx;
}

static bool mpu_read_regs(uint8_t reg, uint8_t *buf, size_t len) {
    i2c_start();
    bool ok = i2c_write_byte((uint8_t)(0x68u << 1));
    ok = ok && i2c_write_byte(reg);
    if (ok) {
        i2c_start();
        ok = i2c_write_byte((uint8_t)((0x68u << 1) | 1u));
        for (size_t i = 0; ok && i < len; ++i) {
            buf[i] = i2c_read_byte(i + 1u < len);
        }
    }
    i2c_stop();
    return ok;
}

static bool mpu_write_reg(uint8_t reg, uint8_t value) {
    i2c_start();
    bool ok = i2c_write_byte((uint8_t)(0x68u << 1));
    ok = ok && i2c_write_byte(reg);
    ok = ok && i2c_write_byte(value);
    i2c_stop();
    return ok;
}

static bool mpu_init(void) {
    uint8_t id = 0u;

    if (!mpu_read_regs(0x75u, &id, 1u) || id != 0x68u) {
        return false;
    }
    if (!mpu_write_reg(0x6Bu, 0x01u)) { /* 唤醒，时钟取 PLL */
        return false;
    }
    (void)mpu_write_reg(0x19u, 0u);    /* 采样率 1 kHz */
    (void)mpu_write_reg(0x1Au, 0x03u); /* DLPF 44 Hz */
    return true;
}

static bool g_sensor_ready;

bool sensor_read_raw(int16_t *ax, int16_t *ay, int16_t *az) {
    uint8_t buf[6];

    if (!g_sensor_ready || !mpu_read_regs(0x3Bu, buf, sizeof buf)) {
        return false;
    }
    *ax = (int16_t)((int32_t)(int16_t)((uint16_t)buf[0] << 8 | buf[1]) * 1000 / 16384);
    *ay = (int16_t)((int32_t)(int16_t)((uint16_t)buf[2] << 8 | buf[3]) * 1000 / 16384);
    *az = (int16_t)((int32_t)(int16_t)((uint16_t)buf[4] << 8 | buf[5]) * 1000 / 16384);
    return true;
}

/* ---------- W25Q64：SPI1 + PA4 片选 ---------- */

#define W25X_SECTOR_SIZE 4096u
#define W25X_PAGE_SIZE 256u
#define W25X_CMD_READ 0x03u
#define W25X_CMD_PAGE_PROGRAM 0x02u
#define W25X_CMD_WRITE_ENABLE 0x06u
#define W25X_CMD_READ_STATUS1 0x05u
#define W25X_CMD_SECTOR_ERASE 0x20u

static void spi_select(int on) {
    if (on != 0) {
        GPIOA->BSRR = GPIO_BSRR_BR4;
    } else {
        GPIOA->BSRR = GPIO_BSRR_BS4;
    }
}

static uint8_t spi_xfer(uint8_t out) {
    while ((SPI1->SR & SPI_SR_TXE) == 0) {
    }
    SPI1->DR = out;
    while ((SPI1->SR & SPI_SR_RXNE) == 0) {
    }
    return (uint8_t)SPI1->DR;
}

static void w25x_cmd(uint8_t cmd, uint32_t addr) {
    spi_xfer(cmd);
    spi_xfer((uint8_t)(addr >> 16));
    spi_xfer((uint8_t)(addr >> 8));
    spi_xfer((uint8_t)addr);
}

static void w25x_write_enable(void) {
    spi_select(1);
    (void)spi_xfer(W25X_CMD_WRITE_ENABLE);
    spi_select(0);
}

static void w25x_wait_busy(void) {
    spi_select(1);
    (void)spi_xfer(W25X_CMD_READ_STATUS1);
    while ((spi_xfer(0xFFu) & 0x01u) != 0u) {
    }
    spi_select(0);
}

static void spi_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_SPI1EN;

    /* PA7 = 0xB（MOSI 复用推挽 50 MHz）、PA6 = 0x4（MISO 浮空输入）、
     * PA5 = 0xB（SCK 复用推挽 50 MHz）、PA4 = 0x2（片选推挽输出 2 MHz） */
    GPIOA->CRL = (GPIOA->CRL & ~0xFFFF0000u) | 0xB4B20000u;

    /* SPI1：主机，fPCLK2 / 8 = 9 MHz，模式 0 */
    SPI1->CR1 = 0;
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_1 | SPI_CR1_SPE;
    GPIOA->BSRR = GPIO_BSRR_BS4;
}

void board_lfs_attach(struct lfs_config *cfg) {
    (void)cfg; /* 目标板上块设备地址是固定的，不需要上下文 */
}

int board_lfs_read(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off,
                   void *buffer, lfs_size_t size) {
    (void)cfg;
    uint32_t addr = 0x030000u + (uint32_t)block * W25X_SECTOR_SIZE + (uint32_t)off;
    uint8_t *p = (uint8_t *)buffer;

    spi_select(1);
    w25x_cmd(W25X_CMD_READ, addr);
    for (lfs_size_t i = 0; i < size; ++i) {
        p[i] = spi_xfer(0xFFu);
    }
    spi_select(0);
    return 0;
}

int board_lfs_prog(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off,
                   const void *buffer, lfs_size_t size) {
    (void)cfg;
    uint32_t addr = 0x030000u + (uint32_t)block * W25X_SECTOR_SIZE + (uint32_t)off;
    const uint8_t *p = (const uint8_t *)buffer;

    /* littlefs 的 prog_size 是 16，一页 256 字节，一次写不会跨页 */
    if ((uint32_t)off + (uint32_t)size > W25X_PAGE_SIZE) {
        return -1;
    }

    w25x_write_enable();
    spi_select(1);
    w25x_cmd(W25X_CMD_PAGE_PROGRAM, addr);
    for (lfs_size_t i = 0; i < size; ++i) {
        (void)spi_xfer(p[i]);
    }
    spi_select(0);
    w25x_wait_busy();
    return 0;
}

int board_lfs_erase(const struct lfs_config *cfg, lfs_block_t block) {
    (void)cfg;
    uint32_t addr = 0x030000u + (uint32_t)block * W25X_SECTOR_SIZE;

    w25x_write_enable();
    spi_select(1);
    w25x_cmd(W25X_CMD_SECTOR_ERASE, addr);
    spi_select(0);
    w25x_wait_busy();
    return 0;
}

int board_lfs_sync(const struct lfs_config *cfg) {
    (void)cfg;
    return 0;
}

/* ---------- 看门狗 ---------- */

void board_watchdog_feed(void) {
    IWDG->KR = 0xAAAAu;
}

static void iwdg_init(void) {
    /* LSI 约 40 kHz，64 分频，重装值 1249：4 × 2^4 × 1250 / 40 kHz = 2.0 s */
    IWDG->KR = 0x5555u; /* 允许写 PR/RLR */
    IWDG->PR = 0x04u;   /* 64 分频 */
    IWDG->RLR = 1249u;
    IWDG->KR = 0xAAAAu;
    IWDG->KR = 0xCCCCu; /* 启动 */
}

/* ---------- 入口 ---------- */

static void led_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->BSRR = GPIO_BSRR_BS13;
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x2u << 20); /* 2 MHz 推挽输出 */
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
    (void)task;
    (void)name;
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vApplicationMallocFailedHook(void) {
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

int main(void) {
    clock_init_72mhz();
    led_init();
    console_init_uart();
    i2c_init_pins();
    spi_init();
    iwdg_init();

    g_sensor_ready = mpu_init();
    recorder_console_printf("姿态记录仪：STM32F103 目标构建，传感器%s\n",
                            g_sensor_ready ? "就绪" : "无应答");

    recorder_start();
    vTaskStartScheduler();
    for (;;) {
    }
}