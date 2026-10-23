/*
 * log.c：littlefs 上的 CSV 日志，按大小轮转。
 *
 * littlefs 的移植（块设备、配置、crc）在 3.9 讲过，这里只关心上层：
 * 一条记录一行，写满 LOG_FILE_LIMIT 就换下一个文件，最多 LOG_MAX_FILES 个，
 * 再写就回到 0 覆盖最旧的那个。
 *
 * 打开 LFS_THREADSAFE 后，lfs 的每次调用都在 lock/unlock 回调里；这里把
 * 回调接到一把递归互斥量上，记录任务与 dump 命令可以同时使用文件系统。
 */
#include "log.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "board.h"
#include "lfs.h"

static lfs_t g_lfs;
static SemaphoreHandle_t g_lfs_mutex;
static int g_current;
static lfs_size_t g_current_size;

static int lfs_lock(const struct lfs_config *cfg) {
    (void)cfg;
    return xSemaphoreTakeRecursive(g_lfs_mutex, portMAX_DELAY) == pdTRUE ? 0 : -1;
}

static int lfs_unlock(const struct lfs_config *cfg) {
    (void)cfg;
    return xSemaphoreGiveRecursive(g_lfs_mutex) == pdTRUE ? 0 : -1;
}

/* 分区从 W25Q64 的 0x030000 起；模拟器上由 lfs_rambd 提供 64 个 4 KB 块 */
static struct lfs_config k_lfs_cfg = {
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
    .lock = lfs_lock,
    .unlock = lfs_unlock,
};

static int make_name(char *buf, size_t size, int index) {
    return snprintf(buf, size, "/log_%03d.csv", index);
}

/* 格式化整个分区。目标板上只在挂载失败时调用，会丢掉全部日志。 */
int log_format(void) {
    board_lfs_attach(&k_lfs_cfg);
    if (g_lfs_mutex == NULL) {
        g_lfs_mutex = xSemaphoreCreateRecursiveMutex();
        if (g_lfs_mutex == NULL) {
            return -1;
        }
    }
    return lfs_format(&g_lfs, &k_lfs_cfg);
}

int log_init(void) {
    board_lfs_attach((struct lfs_config *)&k_lfs_cfg);
    if (g_lfs_mutex == NULL) {
        g_lfs_mutex = xSemaphoreCreateRecursiveMutex();
        if (g_lfs_mutex == NULL) {
            return -1;
        }
    }

    int err = lfs_mount(&g_lfs, &k_lfs_cfg);
    if (err != LFS_ERR_OK) {
        /* 第一次上电或格式不对：格式化再挂载 */
        err = lfs_format(&g_lfs, &k_lfs_cfg);
        if (err == LFS_ERR_OK) {
            err = lfs_mount(&g_lfs, &k_lfs_cfg);
        }
    }
    if (err != LFS_ERR_OK) {
        return err;
    }

    /* 启动时找到编号最大、还没写满的那个文件接着写 */
    g_current = 0;
    g_current_size = 0;
    for (int i = 0; i < LOG_MAX_FILES; ++i) {
        char name[24];
        struct lfs_info info;
        make_name(name, sizeof name, i);
        if (lfs_stat(&g_lfs, name, &info) == LFS_ERR_OK && info.size > 0) {
            g_current = i;
            g_current_size = (lfs_size_t)info.size;
        }
    }
    return 0;
}

int log_append(const char *line) {
    char name[24];

    if (g_current_size >= LOG_FILE_LIMIT) {
        g_current = (g_current + 1) % LOG_MAX_FILES;
        g_current_size = 0;
        /* 覆盖旧文件：lfs_remove 之后 lfs_open 会新建 */
        make_name(name, sizeof name, g_current);
        (void)lfs_remove(&g_lfs, name);
    }

    make_name(name, sizeof name, g_current);
    lfs_file_t file;
    int err = lfs_file_open(&g_lfs, &file, name,
                            LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND);
    if (err != LFS_ERR_OK) {
        return err;
    }
    lfs_ssize_t written = lfs_file_write(&g_lfs, &file, line, strlen(line));
    err = lfs_file_close(&g_lfs, &file);
    if (written < 0) {
        return (int)written;
    }
    g_current_size += (lfs_size_t)written;
    return err;
}

int log_read_file(int index, char *out, size_t max, size_t *out_len) {
    char name[24];
    make_name(name, sizeof name, index);

    lfs_file_t file;
    int err = lfs_file_open(&g_lfs, &file, name, LFS_O_RDONLY);
    if (err != LFS_ERR_OK) {
        return err;
    }
    lfs_ssize_t n = lfs_file_read(&g_lfs, &file, out, max - 1u);
    (void)lfs_file_close(&g_lfs, &file);
    if (n < 0) {
        return (int)n;
    }
    out[n] = '\0';
    if (out_len != NULL) {
        *out_len = (size_t)n;
    }
    return 0;
}

int log_erase_all(void) {
    for (int i = 0; i < LOG_MAX_FILES; ++i) {
        char name[24];
        make_name(name, sizeof name, i);
        (void)lfs_remove(&g_lfs, name);
    }
    g_current = 0;
    g_current_size = 0;
    return 0;
}

int log_file_count(void) {
    return LOG_MAX_FILES;
}