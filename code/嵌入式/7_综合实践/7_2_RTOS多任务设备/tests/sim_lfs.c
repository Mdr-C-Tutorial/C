/*
 * sim_lfs.c：test_log 用的板级替身。
 *
 * 只提供 littlefs 的四个块设备回调（接到 lfs_rambd）和 recorder 里
 * 记录任务需要的两个符号，不启动任何任务，也不链接 FreeRTOS。
 */
#include <stdbool.h>
#include <string.h>

#include "lfs.h"
#include "lfs_rambd.h"

/* RAM 块设备：64 个 4 KB 块，由 lfs_rambd 自己分配。
 * lfs_rambd 从 cfg->context 取自己的状态，所以 context 必须指向 lfs_rambd_t。 */
static struct lfs_rambd_config g_rambd_cfg;
static lfs_rambd_t g_rambd;

int rambd_setup(struct lfs_config *cfg) {
    g_rambd_cfg = (struct lfs_rambd_config){
        .read_size = cfg->read_size,
        .prog_size = cfg->prog_size,
        .erase_size = cfg->block_size,
        .erase_count = cfg->block_count,
        .buffer = NULL,
    };
    cfg->context = &g_rambd;
    return lfs_rambd_create(cfg, &g_rambd_cfg);
}

int board_lfs_read(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off,
                   void *buffer, lfs_size_t size) {
    return lfs_rambd_read(cfg, block, off, buffer, size);
}

int board_lfs_prog(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off,
                   const void *buffer, lfs_size_t size) {
    return lfs_rambd_prog(cfg, block, off, buffer, size);
}

int board_lfs_erase(const struct lfs_config *cfg, lfs_block_t block) {
    return lfs_rambd_erase(cfg, block);
}

int board_lfs_sync(const struct lfs_config *cfg) {
    return lfs_rambd_sync(cfg);
}