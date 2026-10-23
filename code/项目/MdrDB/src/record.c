/*
 * record.c：varint 与记录的编码、解码。
 */
#include "record.h"

#include <string.h>

/*
 * varint：每字节低 7 位存数据，最高位为 1 表示后面还有字节，低位组
 * 在前（与 Protocol Buffers 相同）。小于 128 的数只占 1 字节，名字和
 * 邮箱的长度大多在这个范围内，比固定用 2 字节省空间。
 */
size_t varint_encode(uint32_t value, uint8_t *out) {
    size_t n = 0;
    while (value >= 0x80) {
        out[n++] = (uint8_t)(value | 0x80);
        value >>= 7;
    }
    out[n++] = (uint8_t)value;
    return n;
}

size_t varint_decode(const uint8_t *in, size_t size, uint32_t *value) {
    uint32_t result = 0;
    for (size_t i = 0; i < size && i < VARINT_MAX_SIZE; i++) {
        uint32_t bits = in[i] & 0x7Fu;
        if (i == VARINT_MAX_SIZE - 1 && bits > 0x0Fu) {
            return 0; /* 第 5 个字节只剩 4 位可用，再多就超出 32 位 */
        }
        result |= bits << (7 * i);
        if ((in[i] & 0x80u) == 0) {
            *value = result;
            return i + 1;
        }
    }
    return 0;
}

/*
 * 求字段长度。调用者可能把数组填满而没有 '\0'，所以用 memchr 在数组
 * 范围内找结尾，而不是 strlen。
 */
static db_status text_length(const char *text, size_t capacity, size_t *len) {
    const char *end = memchr(text, '\0', capacity);
    if (end == NULL) {
        return DB_TOO_LONG;
    }
    *len = (size_t)(end - text);
    return DB_OK;
}

static uint8_t *put_text(uint8_t *out, const char *text, size_t len) {
    out += varint_encode((uint32_t)len, out);
    memcpy(out, text, len);
    return out + len;
}

db_status record_encode(const db_row *row, uint8_t *out, size_t *size) {
    size_t name_len;
    size_t email_len;
    if (text_length(row->name, sizeof row->name, &name_len) != DB_OK ||
        text_length(row->email, sizeof row->email, &email_len) != DB_OK) {
        return DB_TOO_LONG;
    }
    uint8_t *end = put_text(out, row->name, name_len);
    end = put_text(end, row->email, email_len);
    *size = (size_t)(end - out);
    return DB_OK;
}

/* 读一个文本字段。长度是否超限、是否越过记录末尾、内容里有没有 '\0'，
 * 逐项核对：文件里的任何数字都不可信。 */
static db_status get_text(const uint8_t *in, size_t size, size_t *pos,
                          char *out, size_t max) {
    uint32_t len;
    size_t n = varint_decode(in + *pos, size - *pos, &len);
    if (n == 0 || len > max || len > size - *pos - n) {
        return DB_CORRUPT;
    }
    const uint8_t *text = in + *pos + n;
    if (memchr(text, '\0', len) != NULL) {
        return DB_CORRUPT; /* 文本里有 '\0'，读出来会被截断 */
    }
    memcpy(out, text, len);
    out[len] = '\0';
    *pos += n + len;
    return DB_OK;
}

db_status record_decode(const uint8_t *in, size_t size, db_row *row) {
    size_t pos = 0;
    db_status rc = get_text(in, size, &pos, row->name, DB_NAME_MAX);
    if (rc == DB_OK) {
        rc = get_text(in, size, &pos, row->email, DB_EMAIL_MAX);
    }
    if (rc == DB_OK && pos != size) {
        rc = DB_CORRUPT; /* 记录末尾多出了字节 */
    }
    return rc;
}
