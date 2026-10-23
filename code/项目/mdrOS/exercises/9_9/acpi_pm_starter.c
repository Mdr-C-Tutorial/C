/* 9.9 习题 50053 的起手代码：在主机上模拟通过 FADT 里的端口做电源管理。
 *
 * 编译：gcc -std=c17 -Wall -Wextra acpi_pm_starter.c -o acpi_pm
 *
 * 需要补全 acpi_enable() 与 acpi_shutdown()。模拟器的行为：
 *   - 向 SMI_CMD（0xB2，8 位）写入 ACPI_ENABLE 后，PM1a_CNT 的 SCI_EN 位
 *     （bit 0）不会立刻置位，要再读三次才变成 1，模拟固件处理需要时间；
 *   - PM1a_CNT（0x604，16 位）可读可写；
 *   - PM_TMR（0x608，32 位）每次读加 1。
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "acpi_fixture.h"

#define SMI_CMD_PORT 0xB2
#define PM1A_CNT_PORT 0x604
#define PM_TMR_PORT 0x608

#define __unused __attribute__((unused))

static uint16_t pm1_cnt;
static uint32_t pm_tmr;
static int enable_pending;
static int poll_count;
static int writes;

static __unused void out8(uint16_t port, uint8_t value) {
    printf("  out8  0x%03X <- 0x%02X\n", port, value);
    writes++;
    if (port == SMI_CMD_PORT && value == fadt_table[0x34]) {
        enable_pending = 1;
        poll_count = 0;
    }
}

static __unused uint8_t in8(uint16_t port) {
    (void)port;
    return 0;
}

static __unused void out16(uint16_t port, uint16_t value) {
    printf("  out16 0x%03X <- 0x%04X\n", port, value);
    writes++;
    if (port == PM1A_CNT_PORT) {
        pm1_cnt = value;
    }
}

static __unused uint16_t in16(uint16_t port) {
    if (port == PM1A_CNT_PORT) {
        if (enable_pending && ++poll_count >= 3) {
            pm1_cnt |= 0x0001; /* SCI_EN：使能命令生效 */
            enable_pending = 0;
        }
        printf("  in16  0x%03X -> 0x%04X\n", port, pm1_cnt);
        return pm1_cnt;
    }
    return 0;
}

static __unused uint32_t in32(uint16_t port) {
    if (port == PM_TMR_PORT) {
        pm_tmr++;
    }
    return pm_tmr;
}

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void dump_fadt(void) {
    printf("FADT:\n");
    printf("  DSDT       = 0x%05X\n", rd32(fadt_table + 0x28));
    printf("  SCI_INT    = %u\n", (unsigned)(fadt_table[0x2E] | (fadt_table[0x2F] << 8)));
    printf("  SMI_CMD    = 0x%03X\n", rd32(fadt_table + 0x30));
    printf("  ACPI_ENABLE= 0x%02X\n", fadt_table[0x34]);
    printf("  PM1a_CNT   = 0x%03X\n", rd32(fadt_table + 0x40));
    printf("  PM1_CNT_LEN= %u\n", fadt_table[0x59]);
    printf("  PM_TMR     = 0x%03X\n", rd32(fadt_table + 0x4C));
}

/* TODO：向 SMI_CMD 写 ACPI_ENABLE，然后轮询 PM1a_CNT 的 bit 0（SCI_EN），
 * 最多 1000 次。成功返回 0，超时返回 -1。 */
int acpi_enable(void) {
    return -1;
}

/* TODO：读 PM1a_CNT，保留低 13 位，把 SLP_TYP 置为 5、SLP_EN 置 1，写回。
 * SLP_TYP 在 bit 10～12，SLP_EN 在 bit 13。 */
void acpi_shutdown(void) {
}

int main(void) {
    dump_fadt();
    pm1_cnt = 0x0000;
    if (acpi_enable() != 0) {
        printf("acpi_enable failed\n");
        return 1;
    }
    printf("after enable: PM1a_CNT=0x%04X, writes=%d, polls=%d\n", pm1_cnt, writes, poll_count);
    acpi_shutdown();
    printf("after shutdown: PM1a_CNT=0x%04X, PM_TMR=%u\n", pm1_cnt, in32(PM_TMR_PORT));
    return 0;
}
