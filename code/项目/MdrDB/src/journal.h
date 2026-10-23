/*
 * journal.h：回滚日志（rollback journal）。
 *
 * 事务第一次修改某页之前，先把这页的原始内容追加到日志里；事务提交时
 * 删除日志。进程在提交前崩溃，下次打开数据库时日志还在，把其中的原始
 * 页写回数据库文件，数据库就回到事务开始前的样子。
 *
 * 日志文件格式（整数为大端序）：
 *   文件头 16 字节：魔数 "MDBJRNL1"（8）、页大小（4）、
 *                   前 12 字节的 CRC-32（4）
 *   之后是若干条记录，每条：页号（4）、页的原始内容（DB_PAGE_SIZE）、
 *                   前两项的 CRC-32（4）
 *
 * 日志不记录条数：回放读到文件尾、或者遇到第一条校验和不对的记录就停。
 * 崩溃时正在写的那条记录可能只写了一半，校验和能把它认出来。
 */
#ifndef MDRDB_JOURNAL_H
#define MDRDB_JOURNAL_H

#include "file.h"
#include "format.h"

#include <stdbool.h>
#include <stdint.h>

#define JOURNAL_HEADER_SIZE 16
#define JOURNAL_RECORD_SIZE (4 + DB_PAGE_SIZE + 4)

typedef struct journal {
    db_file *file;   /* NULL 表示日志没有打开 */
    long end;        /* 下一条记录写在哪里 */
    bool needs_sync; /* 有记录写入后还没同步 */
} journal;

db_status journal_create(journal *j, const char *path);
db_status journal_append(journal *j, uint32_t pgno, const uint8_t *data);
db_status journal_sync(journal *j);
void journal_close(journal *j);

/*
 * 把日志里每条完好的记录写回数据库文件，*restored 返回写回的页数。
 * 事务回滚和打开数据库时的崩溃恢复都用它。
 */
db_status journal_playback(const char *path, db_file *dest, uint32_t *restored);

#endif
