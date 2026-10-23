/*
 * record.h：记录编码。
 *
 * 记录（record）是一行里除主键以外的字段编码成的字节串，存在 B+ 树
 * 叶子单元格的负载里。主键 id 已经是单元格的键，不再重复存放。
 *
 * 格式：name 的长度（varint）、name 的字节、email 的长度（varint）、
 * email 的字节。文本不带结尾的 '\0'。
 */
#ifndef MDRDB_RECORD_H
#define MDRDB_RECORD_H

#include "mdrdb.h"

#include <stddef.h>
#include <stdint.h>

#define VARINT_MAX_SIZE 5 /* 32 位整数最多 5 字节 */
#define RECORD_MAX_SIZE (2 * VARINT_MAX_SIZE + DB_NAME_MAX + DB_EMAIL_MAX)

size_t varint_encode(uint32_t value, uint8_t *out);
/* 返回读掉的字节数；输入被截断或超出 32 位时返回 0。 */
size_t varint_decode(const uint8_t *in, size_t size, uint32_t *value);

/* out 至少 RECORD_MAX_SIZE 字节。字段过长时返回 DB_TOO_LONG。 */
db_status record_encode(const db_row *row, uint8_t *out, size_t *size);
/* 只填 name 和 email；数据不合格式时返回 DB_CORRUPT。 */
db_status record_decode(const uint8_t *in, size_t size, db_row *row);

#endif
