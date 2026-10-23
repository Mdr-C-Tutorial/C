/*
 * file.h：文件层。数据库对文件的所有访问都经过这里。
 *
 * 这一层只用标准 C 的 stdio，Windows 和 Linux 共用同一份代码。
 * 除了按偏移读写整块数据，它还提供故障注入（fault injection）：
 * 让某一次写操作"崩溃"，用来测试崩溃恢复。
 */
#ifndef MDRDB_FILE_H
#define MDRDB_FILE_H

#include "mdrdb.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct db_file db_file;

db_status file_open(const char *path, db_file **out);   /* 打开已有文件 */
db_status file_create(const char *path, db_file **out); /* 新建或清空 */
void file_close(db_file *f);

/* 从 offset 处读 size 字节；文件不够长时 *nread 小于 size。 */
db_status file_read_at(db_file *f, long offset, void *buf, size_t size,
                       size_t *nread);
db_status file_write_at(db_file *f, long offset, const void *buf, size_t size);
db_status file_sync(db_file *f);

bool file_exists(const char *path);
db_status file_remove(const char *path);

/*
 * 故障注入。"写操作"指 file_create、file_write_at 和 file_remove，
 * 它们会改变磁盘上的状态。fault_arm(n, mode) 之后，再有 n 次写操作
 * 照常完成，第 n + 1 次就是崩溃点：
 *   FAULT_EXIT          进程立即退出，用于命令行里的演示；
 *   FAULT_RETURN_ERROR  这次和之后的写操作都失败且不改动磁盘，
 *                       相当于进程已经死去，用于进程内的测试。
 * 崩溃点上的 file_write_at 只写进一半数据，模拟"撕裂写"。
 */
typedef enum fault_mode { FAULT_RETURN_ERROR, FAULT_EXIT } fault_mode;

void fault_arm(long writes_allowed, fault_mode mode);
void fault_disarm(void);
bool fault_crashed(void);
long fault_write_count(void); /* 程序启动以来完成的写操作次数 */

#endif
