#define LFS_THREADSAFE 1

#include <stdio.h>
#include <string.h>

#include "lfs.h"

/* littlefs 需要的锁：主机测试单线程，空实现 */
static int lock_stub(const struct lfs_config *cfg) {
    (void)cfg;
    return 0;
}

static int unlock_stub(const struct lfs_config *cfg) {
    (void)cfg;
    return 0;
}

/* 由 sim_lfs.c 提供，内部接 lfs_rambd */
int board_lfs_read(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off,
                   void *buffer, lfs_size_t size);
int board_lfs_prog(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off,
                   const void *buffer, lfs_size_t size);
int board_lfs_erase(const struct lfs_config *cfg, lfs_block_t block);
int board_lfs_sync(const struct lfs_config *cfg);
int rambd_setup(struct lfs_config *cfg);

static char g_read_buf[512];
static char g_prog_buf[512];
static char g_lookahead[16];

static struct lfs_config k_cfg = {
    .read = board_lfs_read,
    .prog = board_lfs_prog,
    .erase = board_lfs_erase,
    .sync = board_lfs_sync,
    .read_size = 16,
    .prog_size = 16,
    .block_size = 4096,
    .block_count = 64,
    .cache_size = 512,
    .lookahead_size = 16,
    .block_cycles = 200,
    .lock = lock_stub,
    .unlock = unlock_stub,
    .read_buffer = g_read_buf,
    .prog_buffer = g_prog_buf,
    .lookahead_buffer = g_lookahead,
};

int main(void) {
    rambd_setup(&k_cfg);

    lfs_t lfs;
    lfs_file_t file;
    char buf[64];
    int err = lfs_format(&lfs, &k_cfg);
    printf("[1] format: %d\n", err);
    err = lfs_mount(&lfs, &k_cfg);
    printf("[2] mount: %d\n", err);

    /* 写入 3 行，读回来核对 */
    printf("[3] write /a.csv and read back\n");
    (void)lfs_file_open(&lfs, &file, "/a.csv", LFS_O_WRONLY | LFS_O_CREAT);
    (void)lfs_file_write(&lfs, &file, "1,10,20,30,0,0\n", 15);
    (void)lfs_file_write(&lfs, &file, "2,11,21,31,1,2\n", 15);
    (void)lfs_file_close(&lfs, &file);

    (void)lfs_file_open(&lfs, &file, "/a.csv", LFS_O_RDONLY);
    lfs_ssize_t n = lfs_file_read(&lfs, &file, buf, sizeof buf - 1);
    (void)lfs_file_close(&lfs, &file);
    buf[n] = '\0';
    printf("    read %d bytes:\n%s", (int)n, buf);

    /* 断电模拟：重新挂载后数据还在 */
    printf("[4] remount and stat\n");
    (void)lfs_unmount(&lfs);
    (void)lfs_mount(&lfs, &k_cfg);
    struct lfs_info info;
    err = lfs_stat(&lfs, "/a.csv", &info);
    printf("    stat: err=%d size=%u\n", err, (unsigned)info.size);

    if (strcmp(buf, "1,10,20,30,0,0\n2,11,21,31,1,2\n") == 0 && err == 0) {
        printf("\nall tests passed\n");
        return 0;
    }
    printf("\nFAILED\n");
    return 1;
}