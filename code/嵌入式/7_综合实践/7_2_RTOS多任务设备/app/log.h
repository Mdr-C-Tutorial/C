#ifndef LOG_H
#define LOG_H

#include <stddef.h>
#include <stdint.h>

#include "recorder.h"

/*
 * 记录层：在 littlefs 上按大小轮转的 CSV 文件。
 *
 * 目录 /log 下依次是 log_000.csv、log_001.csv……写满一个就换下一个，
 * 最多保留 LOG_MAX_FILES 个，最旧的被覆盖。所有函数只在记录任务里调用。
 */
#define LOG_MAX_FILES 4
#define LOG_FILE_LIMIT 4096u

/* 只格式化不挂载：模拟器每次启动都是空白 RAM，用它代替“出厂已写入文件系统”。 */
int log_format(void);

int log_init(void);
int log_append(const char *line);
int log_read_file(int index, char *out, size_t max, size_t *out_len);
int log_erase_all(void);
int log_file_count(void);

#endif /* LOG_H */