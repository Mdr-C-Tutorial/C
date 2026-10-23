#include "bd/lfs_rambd.h"
#include "lfs.h"
#include <stdint.h>
#include <stdio.h>

static lfs_rambd_t g_bd;

static const struct lfs_rambd_config g_bd_cfg = {
    .read_size = 16,
    .prog_size = 256,
    .erase_size = 4096,
    .erase_count = 128,
    .buffer = NULL,
};

static const struct lfs_config g_lfs_cfg = {
    .context = &g_bd,
    .read = lfs_rambd_read,
    .prog = lfs_rambd_prog,
    .erase = lfs_rambd_erase,
    .sync = lfs_rambd_sync,
    .read_size = 16,
    .prog_size = 256,
    .block_size = 4096,
    .block_count = 128,
    .cache_size = 256,
    .lookahead_size = 16,
    .block_cycles = 500,
};

int main(void) {
    if (lfs_rambd_create(&g_lfs_cfg, &g_bd_cfg) != 0) {
        return 1;
    }

    for (int boot = 1; boot <= 3; ++boot) {
        lfs_t lfs;
        lfs_file_t file;

        int err = lfs_mount(&lfs, &g_lfs_cfg);
        if (err != 0) {
            lfs_format(&lfs, &g_lfs_cfg);
            lfs_mount(&lfs, &g_lfs_cfg);
        }

        uint32_t boot_count = 0;
        lfs_file_open(&lfs, &file, "boot_count", LFS_O_RDWR | LFS_O_CREAT);
        lfs_file_read(&lfs, &file, &boot_count, sizeof(boot_count));

        boot_count += 1;
        lfs_file_rewind(&lfs, &file);
        lfs_file_write(&lfs, &file, &boot_count, sizeof(boot_count));
        lfs_file_close(&lfs, &file);

        lfs_unmount(&lfs);
        printf("[boot #%d] boot_count = %u\n", boot, (unsigned)boot_count);
    }

    lfs_rambd_destroy(&g_lfs_cfg);
    return 0;
}
