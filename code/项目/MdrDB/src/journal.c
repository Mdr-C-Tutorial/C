/*
 * journal.c：回滚日志的写入与回放。格式见 journal.h。
 */
#include "journal.h"

#include <limits.h>
#include <string.h>

static const uint8_t journal_magic[8] = {'M', 'D', 'B', 'J',
                                         'R', 'N', 'L', '1'};

/*
 * CRC-32（IEEE 802.3 多项式，与 zlib 的 crc32 结果相同）。逐位计算，
 * 比查表慢，但只有几行；每条日志记录只算一次，速度不成问题。
 */
static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t size) {
    crc = ~crc;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            uint32_t mask = 0u - (crc & 1u); /* 最低位为 1 时全 1 */
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

db_status journal_create(journal *j, const char *path) {
    uint8_t header[JOURNAL_HEADER_SIZE];
    memcpy(header, journal_magic, sizeof journal_magic);
    put_u32(header + 8, DB_PAGE_SIZE);
    put_u32(header + 12, crc32_update(0, header, 12));

    j->file = NULL;
    j->end = 0;
    j->needs_sync = false;
    db_status rc = file_create(path, &j->file);
    if (rc != DB_OK) {
        return rc;
    }
    rc = file_write_at(j->file, 0, header, sizeof header);
    if (rc != DB_OK) {
        journal_close(j);
        return rc;
    }
    j->end = JOURNAL_HEADER_SIZE;
    j->needs_sync = true;
    return DB_OK;
}

db_status journal_append(journal *j, uint32_t pgno, const uint8_t *data) {
    if (j->end > LONG_MAX - JOURNAL_RECORD_SIZE) {
        return DB_FULL; /* 日志的偏移也是 long，不能无限增长 */
    }
    uint8_t record[JOURNAL_RECORD_SIZE];
    put_u32(record, pgno);
    memcpy(record + 4, data, DB_PAGE_SIZE);
    put_u32(record + 4 + DB_PAGE_SIZE,
            crc32_update(0, record, 4 + DB_PAGE_SIZE));

    /* 整条记录一次写出：崩溃只会让最后一条不完整，不会打乱前面的。 */
    db_status rc = file_write_at(j->file, j->end, record, sizeof record);
    if (rc != DB_OK) {
        return rc;
    }
    j->end += JOURNAL_RECORD_SIZE;
    j->needs_sync = true;
    return DB_OK;
}

db_status journal_sync(journal *j) {
    if (!j->needs_sync) {
        return DB_OK;
    }
    db_status rc = file_sync(j->file);
    if (rc == DB_OK) {
        j->needs_sync = false;
    }
    return rc;
}

void journal_close(journal *j) {
    file_close(j->file);
    j->file = NULL;
}

static bool header_is_valid(const uint8_t *header) {
    return memcmp(header, journal_magic, sizeof journal_magic) == 0 &&
           get_u32(header + 8) == DB_PAGE_SIZE &&
           get_u32(header + 12) == crc32_update(0, header, 12);
}

db_status journal_playback(const char *path, db_file *dest,
                           uint32_t *restored) {
    *restored = 0;
    db_file *jf;
    db_status rc = file_open(path, &jf);
    if (rc != DB_OK) {
        return rc;
    }

    uint8_t header[JOURNAL_HEADER_SIZE];
    size_t n;
    rc = file_read_at(jf, 0, header, sizeof header, &n);
    /*
     * 文件头不完整，说明崩溃发生在日志刚创建的时候。数据库文件要等日志
     * 同步之后才会被修改，此时还没动过，没有需要恢复的内容。
     */
    if (rc != DB_OK || n < sizeof header || !header_is_valid(header)) {
        file_close(jf);
        return rc;
    }

    uint8_t record[JOURNAL_RECORD_SIZE];
    long offset = JOURNAL_HEADER_SIZE;
    for (;;) {
        rc = file_read_at(jf, offset, record, sizeof record, &n);
        if (rc != DB_OK || n < sizeof record) {
            break;
        }
        uint32_t crc = get_u32(record + 4 + DB_PAGE_SIZE);
        if (crc != crc32_update(0, record, 4 + DB_PAGE_SIZE)) {
            break; /* 写了一半的记录：它对应的页还没写进数据库文件 */
        }
        uint32_t pgno = get_u32(record);
        if (pgno >= DB_MAX_PAGES) {
            break;
        }
        rc = file_write_at(dest, (long)pgno * DB_PAGE_SIZE, record + 4,
                           DB_PAGE_SIZE);
        if (rc != DB_OK) {
            break;
        }
        (*restored)++;
        offset += JOURNAL_RECORD_SIZE;
    }
    file_close(jf);
    return rc;
}
