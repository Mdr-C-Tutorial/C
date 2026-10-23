#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "recorder.h"

/* 板级与模拟器分别实现的部分 */
bool board_sensor_read(int16_t *ax, int16_t *ay, int16_t *az);
void board_watchdog_feed(void);

/* 控制台输入：取一行，没有时返回 false */
bool board_console_read_line(char *buf, size_t max, TickType_t wait);

/* 结束程序：模拟器里退出进程，目标板上停住 */
void board_exit(void);

/* littlefs 的块设备接口 */
struct lfs_config;

/* 给 littlefs 的配置补上块设备上下文：模拟器上是 lfs_rambd 的状态，
 * 目标板上不需要。log_init 在挂载前调用。 */
void board_lfs_attach(struct lfs_config *cfg);
int board_lfs_read(const struct lfs_config *cfg, uint32_t block, uint32_t off,
                   void *buffer, uint32_t size);
int board_lfs_prog(const struct lfs_config *cfg, uint32_t block, uint32_t off,
                   const void *buffer, uint32_t size);
int board_lfs_erase(const struct lfs_config *cfg, uint32_t block);
int board_lfs_sync(const struct lfs_config *cfg);

#endif /* BOARD_H */