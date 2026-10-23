/*
 * init_export_demo.c：INIT_BOARD_EXPORT、INIT_APP_EXPORT 与 MSH_CMD_EXPORT
 * 把函数指针放进哪些段。
 *
 * 只看段布局，不链接成可运行的固件：
 *   arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -std=gnu11 -O2 -Wall -Wextra \
 *       -ffreestanding -D__RTTHREAD__ -DFINSH_USING_SYMTAB \
 *       -I<rt-thread>/include -I<rt-thread>/components/finsh -c init_export_demo.c
 *   arm-none-eabi-objdump -h init_export_demo.o
 */
#include <rtthread.h>
#include <finsh.h>

static int led_init(void) {
    return 0;
}
INIT_BOARD_EXPORT(led_init);

static int led_app_init(void) {
    return 0;
}
INIT_APP_EXPORT(led_app_init);

static int mdr_stat(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return 0;
}
MSH_CMD_EXPORT(mdr_stat, print sampling statistics);