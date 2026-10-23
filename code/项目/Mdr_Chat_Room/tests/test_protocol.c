#include "protocol.h"

#include "check.h"

#include <string.h>

/* 按给定字段直接拼出一帧，name_len 可以故意写错。返回帧长。 */
static size_t raw_frame(unsigned char *out, unsigned char type,
                        unsigned char name_len, const char *rest) {
    size_t rest_len = strlen(rest);
    size_t body_len = 1 + rest_len;
    out[0] = type;
    out[1] = (unsigned char)(body_len >> 8);
    out[2] = (unsigned char)(body_len & 0xFF);
    out[3] = name_len;
    memcpy(out + 4, rest, rest_len);
    return PROTO_HEADER_SIZE + body_len;
}

static proto_status decode_status(const unsigned char *data, size_t len) {
    message msg;
    size_t frame_len;
    return proto_decode(data, len, &msg, &frame_len);
}

/* 编码结果逐字节对照 protocol.h 里的格式说明：这些就是线路上传送的字节。 */
static void test_encode_layout(void) {
    message msg;
    unsigned char frame[PROTO_FRAME_MAX];

    message_set(&msg, MSG_SAY, NULL, "hi");
    const unsigned char say[] = {0x02, 0x00, 0x03, 0x00, 'h', 'i'};
    CHECK(proto_encode(&msg, frame, sizeof frame) == sizeof say);
    CHECK(memcmp(frame, say, sizeof say) == 0);

    message_set(&msg, MSG_WHISPER, "bob", "hi");
    const unsigned char whisper[] = {0x03, 0x00, 0x06, 0x03, 'b',
                                     'o',  'b',  'h',  'i'};
    CHECK(proto_encode(&msg, frame, sizeof frame) == sizeof whisper);
    CHECK(memcmp(frame, whisper, sizeof whisper) == 0);

    message_set(&msg, MSG_LIST, NULL, NULL);
    const unsigned char list[] = {0x04, 0x00, 0x01, 0x00};
    CHECK(proto_encode(&msg, frame, sizeof frame) == sizeof list);
    CHECK(memcmp(frame, list, sizeof list) == 0);
}

/* 帧体超过 255 字节时，长度的高字节在前（大端序）。 */
static void test_length_is_big_endian(void) {
    char text[301];
    memset(text, 'x', 300);
    text[300] = '\0';
    message msg;
    unsigned char frame[PROTO_FRAME_MAX];
    message_set(&msg, MSG_SAY, NULL, text);
    CHECK(proto_encode(&msg, frame, sizeof frame) == PROTO_HEADER_SIZE + 301);
    CHECK(frame[1] == 0x01 && frame[2] == 0x2D); /* 301 = 0x012D */
}

static void test_round_trip(void) {
    const msg_type types[] = {MSG_NICK,    MSG_SAY,   MSG_WHISPER, MSG_LIST,
                              MSG_WELCOME, MSG_CHAT,  MSG_PRIVATE, MSG_USERS,
                              MSG_NOTICE,  MSG_ERROR};
    for (size_t i = 0; i < sizeof types / sizeof types[0]; i++) {
        message in;
        message out;
        unsigned char frame[PROTO_FRAME_MAX];
        /* UTF-8 的多字节字符不是控制字符，原样通过 */
        message_set(&in, types[i], "alice", "你好, world");
        size_t len = proto_encode(&in, frame, sizeof frame);
        size_t frame_len = 0;
        CHECK(proto_decode(frame, len, &out, &frame_len) == PROTO_OK);
        CHECK(frame_len == len);
        CHECK(out.type == in.type);
        CHECK(strcmp(out.name, "alice") == 0);
        CHECK(strcmp(out.text, "你好, world") == 0);
    }
}

static void test_largest_frame(void) {
    char name[PROTO_NICK_MAX + 1];
    char text[PROTO_TEXT_MAX + 1];
    memset(name, 'n', PROTO_NICK_MAX);
    name[PROTO_NICK_MAX] = '\0';
    memset(text, 't', PROTO_TEXT_MAX);
    text[PROTO_TEXT_MAX] = '\0';

    message in;
    message out;
    unsigned char frame[PROTO_FRAME_MAX];
    message_set(&in, MSG_CHAT, name, text);
    CHECK(proto_encode(&in, frame, sizeof frame) == PROTO_FRAME_MAX);
    size_t frame_len = 0;
    CHECK(proto_decode(frame, sizeof frame, &out, &frame_len) == PROTO_OK);
    CHECK(frame_len == PROTO_FRAME_MAX);
    CHECK(strcmp(out.name, name) == 0 && strcmp(out.text, text) == 0);
    /* 输出缓冲区少一个字节就放不下 */
    CHECK(proto_encode(&in, frame, sizeof frame - 1) == 0);
}

/* 一帧的任何一个真前缀都只能得到 PROTO_INCOMPLETE。 */
static void test_every_prefix_is_incomplete(void) {
    message msg;
    unsigned char frame[PROTO_FRAME_MAX];
    message_set(&msg, MSG_CHAT, "alice", "hello");
    size_t len = proto_encode(&msg, frame, sizeof frame);
    for (size_t prefix = 0; prefix < len; prefix++) {
        CHECK(decode_status(frame, prefix) == PROTO_INCOMPLETE);
    }
}

/* 模拟 TCP 把数据拆得最碎的情况：两帧的字节一个一个地到达。 */
static void test_byte_by_byte(void) {
    unsigned char stream[2 * PROTO_FRAME_MAX];
    message msg;
    message_set(&msg, MSG_SAY, NULL, "first");
    size_t first_len = proto_encode(&msg, stream, sizeof stream);
    message_set(&msg, MSG_SAY, NULL, "second");
    size_t total = first_len + proto_encode(&msg, stream + first_len,
                                            sizeof stream - first_len);

    frame_buffer buf = {.len = 0};
    int got = 0;
    for (size_t i = 0; i < total; i++) {
        buf.data[buf.len++] = stream[i];
        message out;
        proto_status status = frame_buffer_next(&buf, &out);
        if (status == PROTO_OK) {
            got++;
            /* 每一帧恰好在它的最后一个字节到达时被取出 */
            CHECK(i + 1 == (got == 1 ? first_len : total));
            CHECK(strcmp(out.text, got == 1 ? "first" : "second") == 0);
        } else {
            CHECK(status == PROTO_INCOMPLETE);
        }
    }
    CHECK(got == 2);
    CHECK(buf.len == 0);
}

/* 几帧挤在一次 recv 里，后面还跟着半帧。 */
static void test_frames_in_one_read(void) {
    const char *texts[] = {"a", "bb", "ccc"};
    frame_buffer buf = {.len = 0};
    message msg;
    for (int i = 0; i < 3; i++) {
        message_set(&msg, MSG_SAY, NULL, texts[i]);
        buf.len += proto_encode(&msg, buf.data + buf.len,
                                sizeof buf.data - buf.len);
    }
    unsigned char tail[PROTO_FRAME_MAX];
    message_set(&msg, MSG_SAY, NULL, "tail");
    size_t tail_len = proto_encode(&msg, tail, sizeof tail);
    memcpy(buf.data + buf.len, tail, 5); /* 第四帧只到了前 5 个字节 */
    buf.len += 5;

    message out;
    for (int i = 0; i < 3; i++) {
        CHECK(frame_buffer_next(&buf, &out) == PROTO_OK);
        CHECK(strcmp(out.text, texts[i]) == 0);
    }
    CHECK(frame_buffer_next(&buf, &out) == PROTO_INCOMPLETE);
    CHECK(buf.len == 5 && memcmp(buf.data, tail, 5) == 0);

    /* 剩下的字节到达后，半帧补成整帧 */
    memcpy(buf.data + buf.len, tail + 5, tail_len - 5);
    buf.len += tail_len - 5;
    CHECK(frame_buffer_next(&buf, &out) == PROTO_OK);
    CHECK(strcmp(out.text, "tail") == 0);
    CHECK(buf.len == 0);
}

static void test_rejects_bad_frames(void) {
    unsigned char frame[PROTO_FRAME_MAX + 16];
    size_t len;

    /* 只看帧头就能发现的错误，不必等帧体 */
    const unsigned char unknown_type[] = {0x7F, 0x00, 0x01};
    CHECK(decode_status(unknown_type, 3) == PROTO_BAD_TYPE);
    const unsigned char empty_body[] = {0x02, 0x00, 0x00};
    CHECK(decode_status(empty_body, 3) == PROTO_BAD_LENGTH);
    const unsigned char huge_body[] = {0x02, 0xFF, 0xFF};
    CHECK(decode_status(huge_body, 3) == PROTO_BAD_LENGTH);

    /* name_len 超过昵称上限 */
    len = raw_frame(frame, MSG_NICK, 17, "abcdefghijklmnopq");
    CHECK(decode_status(frame, len) == PROTO_BAD_FIELD);
    /* name_len 比帧体剩下的字节还多 */
    len = raw_frame(frame, MSG_NICK, 5, "abc");
    CHECK(decode_status(frame, len) == PROTO_BAD_FIELD);
    /* 控制字符：换行、ESC、DEL、NUL */
    len = raw_frame(frame, MSG_SAY, 0, "two\nlines");
    CHECK(decode_status(frame, len) == PROTO_BAD_FIELD);
    len = raw_frame(frame, MSG_SAY, 0, "\x1b[2J");
    CHECK(decode_status(frame, len) == PROTO_BAD_FIELD);
    len = raw_frame(frame, MSG_SAY, 0, "del\x7f");
    CHECK(decode_status(frame, len) == PROTO_BAD_FIELD);
    const unsigned char nul_in_name[] = {0x01, 0x00, 0x03, 0x02, 'a', 0x00};
    CHECK(decode_status(nul_in_name, sizeof nul_in_name) == PROTO_BAD_FIELD);

    /* 帧体长度在上限之内，但 name 为空时 text 超过了上限 */
    char text[PROTO_TEXT_MAX + 2];
    memset(text, 'x', PROTO_TEXT_MAX + 1);
    text[PROTO_TEXT_MAX + 1] = '\0';
    len = raw_frame(frame, MSG_SAY, 0, text);
    CHECK(decode_status(frame, len) == PROTO_BAD_FIELD);
}

static void test_nick_rules(void) {
    CHECK(nick_is_valid("alice"));
    CHECK(nick_is_valid("Bob_2"));
    CHECK(nick_is_valid("x-y"));
    CHECK(nick_is_valid("abcdefghijklmnop")); /* 16 个字符 */
    CHECK(!nick_is_valid(""));
    CHECK(!nick_is_valid("abcdefghijklmnopq")); /* 17 个字符 */
    CHECK(!nick_is_valid("a b"));
    CHECK(!nick_is_valid("a.b"));
    CHECK(!nick_is_valid("张三"));
}

int main(void) {
    test_encode_layout();
    test_length_is_big_endian();
    test_round_trip();
    test_largest_frame();
    test_every_prefix_is_incomplete();
    test_byte_by_byte();
    test_frames_in_one_read();
    test_rejects_bad_frames();
    test_nick_rules();
    return check_report("test_protocol");
}
