#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*
 * 在主机上用虚拟总线代替 PB6/PB7，验证软件 I2C 的起始、停止、重复起始与
 * 应答位顺序。模拟的 AT24C02（7 位地址 0x50）在内部地址 0x10 处存放 0xA5。
 */
static int g_scl = 1;
static int g_sda_master = 1;
static int g_sda_slave = 1;
static int g_prev_scl = 1;
static int g_prev_sda = 1;
static uint32_t g_clocks = 0;

static int bus_sda(void) {
    /* 开漏“线与”：任一方拉低则总线为 0，双方都释放（1）才由上拉拉为 1 */
    return (g_sda_master != 0 && g_sda_slave != 0) ? 1 : 0;
}

static void record_transition(void) {
    int sda = bus_sda();
    if (g_prev_scl == 1 && g_scl == 1) {
        if (g_prev_sda == 1 && sda == 0) {
            printf("[BUS] START condition  (SCL=1, SDA: 1 -> 0)\n");
        } else if (g_prev_sda == 0 && sda == 1) {
            printf("[BUS] STOP condition   (SCL=1, SDA: 0 -> 1)\n");
        }
    }
    g_prev_scl = g_scl;
    g_prev_sda = sda;
}

static void scl_write(int v) {
    g_scl = v;
    record_transition();
}

static void sda_write(int v) {
    g_sda_master = v;
    record_transition();
}

static int sda_read(void) {
    return bus_sda();
}

static void i2c_start(void) {
    sda_write(1);
    scl_write(1);
    sda_write(0);
    scl_write(0);
}

static void i2c_stop(void) {
    sda_write(0);
    scl_write(1);
    sda_write(1);
}

static bool i2c_write_byte(uint8_t byte, bool slave_will_ack) {
    uint8_t orig = byte;
    char bits[9];
    for (int i = 0; i < 8; ++i) {
        int b = (byte & 0x80u) ? 1 : 0;
        bits[i] = (char)('0' + b);
        sda_write(b);
        scl_write(1);
        g_clocks++;
        scl_write(0);
        byte <<= 1;
    }
    bits[8] = '\0';

    /* 第 9 个时钟：主机写 1 释放 SDA，模拟从机输出 ACK(0) 或 NACK(1) */
    sda_write(1);
    g_sda_slave = slave_will_ack ? 0 : 1;
    scl_write(1);
    g_clocks++;
    bool ack = (sda_read() == 0);
    scl_write(0);
    g_sda_slave = 1;

    printf("[TX ] 0x%02X (%s) -> master SDA=1, bus SDA=%d (%s)\n",
           orig, bits, ack ? 0 : 1, ack ? "ACK" : "NACK");
    return ack;
}

static uint8_t i2c_read_byte(uint8_t slave_tx, bool master_ack) {
    uint8_t rx = 0;
    char bits[9];
    sda_write(1);
    for (int i = 0; i < 8; ++i) {
        g_sda_slave = (slave_tx & (0x80u >> i)) ? 1 : 0;
        scl_write(1);
        g_clocks++;
        int b = sda_read();
        bits[i] = (char)('0' + b);
        rx = (uint8_t)((rx << 1) | b);
        scl_write(0);
    }
    bits[8] = '\0';
    g_sda_slave = 1;

    /* 第 9 个时钟：主机驱动 ACK(0) 或 NACK(1) */
    sda_write(master_ack ? 0 : 1);
    scl_write(1);
    g_clocks++;
    int ack_line = sda_read();
    scl_write(0);
    sda_write(1);

    printf("[RX ] 0x%02X (%s) <- master drives SDA=%d (%s)\n",
           rx, bits, ack_line, master_ack ? "ACK" : "NACK");
    return rx;
}

int main(void) {
    printf("=== AT24C02 random read: dev=0x50, word_addr=0x10 ===\n");
    i2c_start();
    i2c_write_byte((0x50u << 1) | 0u, true);
    i2c_write_byte(0x10u, true);

    i2c_start();
    i2c_write_byte((0x50u << 1) | 1u, true);
    uint8_t val = i2c_read_byte(0xA5u, false);
    i2c_stop();

    printf("Result: read 0x%02X, total SCL pulses = %u, final bus SCL=%d SDA=%d\n",
           val, g_clocks, g_scl, bus_sda());
    return 0;
}
