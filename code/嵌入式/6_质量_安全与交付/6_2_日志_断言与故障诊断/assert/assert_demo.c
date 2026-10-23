/*
 * assert_demo.c：主机上演示断言宏与错误处理的区别。
 *
 * 断言用于“不可能发生”的编程错误；外部输入失败用返回值处理。
 * 编译：gcc -std=c17 -Wall -Wextra -I. assert_demo.c -o assert_demo
 *
 * 讲解见教程 6.2 节「日志、断言与故障诊断」。
 */
#include <stdio.h>
#include <stdlib.h>

#include "mdr_assert.h"

/* 断言失败时主机版打印后以非零状态退出，固件版记录后复位 */
void diag_assert_failed(const char *file, int line, uintptr_t ret) {
    printf("ASSERT FAILED at %s:%d (return address 0x%08lX)\n",
           file, line, (unsigned long)ret);
    fflush(stdout);
    exit(2);
}

/* 编程错误：调用者传了一个不可能的分类号 */
enum sensor_kind { SENSOR_ACCEL = 0, SENSOR_GYRO = 1 };

static int scale_of(enum sensor_kind kind) {
    MDR_ASSERT(kind == SENSOR_ACCEL || kind == SENSOR_GYRO);
    return (kind == SENSOR_ACCEL) ? 1 : 10;
}

/* 外部事件失败：传感器不应答，靠返回值报告 */
static int read_sensor(int (*read_reg)(uint8_t), uint8_t reg, uint8_t *out) {
    int rc = read_reg(reg);
    if (rc < 0) {
        return -1;
    }
    *out = (uint8_t)rc;
    return 0;
}

static int fake_read_reg_ok(uint8_t reg) {
    return (reg == 0x75u) ? 0x68 : -1;
}

int main(int argc, char **argv) {
    (void)argv;
    uint8_t who = 0;

    printf("scale(accel) = %d\n", scale_of(SENSOR_ACCEL));
    printf("scale(gyro)  = %d\n", scale_of(SENSOR_GYRO));

    if (read_sensor(fake_read_reg_ok, 0x75u, &who) == 0) {
        printf("WHO_AM_I = 0x%02X\n", who);
    }
    if (read_sensor(fake_read_reg_ok, 0x6Bu, &who) != 0) {
        printf("read 0x6B failed: sensor did not answer\n");
    }

    if (argc > 1) {
        /* 故意传入非法分类号，触发断言 */
        printf("scale(bad) = %d\n", scale_of((enum sensor_kind)7));
    }
    return 0;
}