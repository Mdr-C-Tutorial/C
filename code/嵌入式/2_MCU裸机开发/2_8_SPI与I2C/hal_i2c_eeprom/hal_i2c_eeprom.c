#include <stdint.h>
#include "stm32f1xx_hal.h"

#define AT24C02_ADDR7    0x50U
#define AT24C02_DEV_ADDR (AT24C02_ADDR7 << 1)

/* --- 寄存器配置 I2C1 主机发送（100 kHz，PCLK1 = 36 MHz） --- */
void i2c1_hw_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    /* PB6（SCL）、PB7（SDA）：50 MHz 复用开漏输出（CNF=11, MODE=11 -> 0xF） */
    GPIOB->CRL = (GPIOB->CRL & ~(0xFFu << 24)) | (0xFFu << 24);

    /* 在 PE=0 时配置时钟控制与最大上升时间 */
    I2C1->CR1 = 0;
    I2C1->CR2 = 36u;   /* PCLK1 = 36 MHz */
    I2C1->CCR = 180u;  /* Sm 100 kHz：Thigh = Tlow = 180 / 36 MHz = 5 us */
    I2C1->TRISE = 37u; /* 1000 ns / (1 / 36 MHz) + 1 = 37 */
    I2C1->CR1 = I2C_CR1_PE;
}

void i2c1_hw_write_byte(uint8_t word_addr, uint8_t data) {
    while ((I2C1->SR2 & I2C_SR2_BUSY) != 0) {
    }

    /* EV5：发送起始条件，等待 SB 置位 */
    I2C1->CR1 |= I2C_CR1_START;
    while ((I2C1->SR1 & I2C_SR1_SB) == 0) {
    }

    /* EV6：写寻址字节（清 SB），等待 ADDR 置位后依次读 SR1、SR2 清 ADDR */
    I2C1->DR = (uint8_t)((AT24C02_ADDR7 << 1) | 0u);
    while ((I2C1->SR1 & I2C_SR1_ADDR) == 0) {
    }
    (void)I2C1->SR1;
    (void)I2C1->SR2;

    /* EV8_1 / EV8：依次写入字地址与数据字节 */
    while ((I2C1->SR1 & I2C_SR1_TXE) == 0) {
    }
    I2C1->DR = word_addr;

    while ((I2C1->SR1 & I2C_SR1_TXE) == 0) {
    }
    I2C1->DR = data;

    /* EV8_2：等待移位寄存器和数据寄存器都空（BTF=1），发送停止条件 */
    while ((I2C1->SR1 & I2C_SR1_BTF) == 0) {
    }
    I2C1->CR1 |= I2C_CR1_STOP;
}

/* --- HAL 方式读写 AT24C02（stm32f1xx-hal-driver 1.1.10） --- */
I2C_HandleTypeDef hi2c1;

HAL_StatusTypeDef i2c1_hal_init(void) {
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 100000U;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0U;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0U;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    return HAL_I2C_Init(&hi2c1);
}

HAL_StatusTypeDef eeprom_hal_write_and_read(uint16_t word_addr,
                                            uint8_t tx_val,
                                            uint8_t *rx_val) {
    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(&hi2c1,
                                             AT24C02_DEV_ADDR,
                                             word_addr,
                                             I2C_MEMADD_SIZE_8BIT,
                                             &tx_val,
                                             1U,
                                             100U);
    if (st != HAL_OK) {
        return st;
    }

    /* 应答轮询：在最多 50 次尝试内等待 EEPROM 内部写周期结束 */
    st = HAL_I2C_IsDeviceReady(&hi2c1, AT24C02_DEV_ADDR, 50U, 100U);
    if (st != HAL_OK) {
        return st;
    }

    return HAL_I2C_Mem_Read(&hi2c1,
                            AT24C02_DEV_ADDR,
                            word_addr,
                            I2C_MEMADD_SIZE_8BIT,
                            rx_val,
                            1U,
                            100U);
}
