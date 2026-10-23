#include "protocol.h"

#include <stdio.h>
#include <string.h>

static bool type_is_known(unsigned char type) {
    switch (type) {
    case MSG_NICK:
    case MSG_SAY:
    case MSG_WHISPER:
    case MSG_LIST:
    case MSG_WELCOME:
    case MSG_CHAT:
    case MSG_PRIVATE:
    case MSG_USERS:
    case MSG_NOTICE:
    case MSG_ERROR:
        return true;
    default:
        return false;
    }
}

static bool field_is_clean(const unsigned char *bytes, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (bytes[i] < 0x20 || bytes[i] == 0x7F) {
            return false;
        }
    }
    return true;
}

void message_set(message *msg, msg_type type, const char *name,
                 const char *text) {
    msg->type = type;
    snprintf(msg->name, sizeof msg->name, "%s", name != NULL ? name : "");
    snprintf(msg->text, sizeof msg->text, "%s", text != NULL ? text : "");
}

size_t proto_encode(const message *msg, unsigned char *out, size_t cap) {
    size_t name_len = strlen(msg->name);
    size_t text_len = strlen(msg->text);
    size_t body_len = 1 + name_len + text_len;
    if (name_len > PROTO_NICK_MAX || text_len > PROTO_TEXT_MAX ||
        PROTO_HEADER_SIZE + body_len > cap) {
        return 0;
    }

    out[0] = (unsigned char)msg->type;
    /* 多字节整数逐字节写出、高位在前。结果与本机的字节序无关，
     * 也不会像直接写出结构体那样把编译器插入的填充字节带上线路。 */
    out[1] = (unsigned char)(body_len >> 8);
    out[2] = (unsigned char)(body_len & 0xFF);

    unsigned char *body = out + PROTO_HEADER_SIZE;
    body[0] = (unsigned char)name_len;
    memcpy(body + 1, msg->name, name_len);
    memcpy(body + 1 + name_len, msg->text, text_len);
    return PROTO_HEADER_SIZE + body_len;
}

proto_status proto_decode(const unsigned char *data, size_t len, message *msg,
                          size_t *frame_len) {
    if (len < PROTO_HEADER_SIZE) {
        return PROTO_INCOMPLETE;
    }
    /* 帧头一到就检查。帧头不对说明对方没有按协议发送，
     * 不必等它把帧体发完（它声称的帧体可能永远不会来）。 */
    if (!type_is_known(data[0])) {
        return PROTO_BAD_TYPE;
    }
    size_t body_len = ((size_t)data[1] << 8) | data[2];
    if (body_len < 1 || body_len > PROTO_BODY_MAX) {
        return PROTO_BAD_LENGTH;
    }
    if (len < PROTO_HEADER_SIZE + body_len) {
        return PROTO_INCOMPLETE;
    }

    const unsigned char *body = data + PROTO_HEADER_SIZE;
    size_t name_len = body[0];
    if (name_len > PROTO_NICK_MAX || 1 + name_len > body_len) {
        return PROTO_BAD_FIELD;
    }
    const unsigned char *name = body + 1;
    const unsigned char *text = name + name_len;
    size_t text_len = body_len - 1 - name_len;
    if (text_len > PROTO_TEXT_MAX || !field_is_clean(name, name_len) ||
        !field_is_clean(text, text_len)) {
        return PROTO_BAD_FIELD;
    }

    msg->type = (msg_type)data[0];
    memcpy(msg->name, name, name_len);
    msg->name[name_len] = '\0';
    memcpy(msg->text, text, text_len);
    msg->text[text_len] = '\0';
    *frame_len = PROTO_HEADER_SIZE + body_len;
    return PROTO_OK;
}

proto_status frame_buffer_next(frame_buffer *buf, message *msg) {
    size_t frame_len;
    proto_status status = proto_decode(buf->data, buf->len, msg, &frame_len);
    if (status == PROTO_OK) {
        /* 把后面的字节挪到开头。缓冲区只有一帧大，搬动的开销可以忽略，
         * 换来的是“数据总从 data[0] 开始”这个简单的约定。 */
        memmove(buf->data, buf->data + frame_len, buf->len - frame_len);
        buf->len -= frame_len;
    }
    return status;
}

const char *proto_status_text(proto_status status) {
    switch (status) {
    case PROTO_OK:
        return "ok";
    case PROTO_INCOMPLETE:
        return "incomplete frame";
    case PROTO_BAD_TYPE:
        return "unknown message type";
    case PROTO_BAD_LENGTH:
        return "bad frame length";
    case PROTO_BAD_FIELD:
        return "malformed field";
    }
    return "unknown status";
}

const char *msg_type_name(msg_type type) {
    switch (type) {
    case MSG_NICK:
        return "NICK";
    case MSG_SAY:
        return "SAY";
    case MSG_WHISPER:
        return "WHISPER";
    case MSG_LIST:
        return "LIST";
    case MSG_WELCOME:
        return "WELCOME";
    case MSG_CHAT:
        return "CHAT";
    case MSG_PRIVATE:
        return "PRIVATE";
    case MSG_USERS:
        return "USERS";
    case MSG_NOTICE:
        return "NOTICE";
    case MSG_ERROR:
        return "ERROR";
    }
    return "?";
}

/* 昵称要能放进 "/msg 昵称 正文" 这种以空格分隔的命令里，显示时也要一眼可辨，
 * 所以只收 ASCII 字母、数字、'_' 和 '-'。 */
bool nick_is_valid(const char *nick) {
    size_t len = strlen(nick);
    if (len == 0 || len > PROTO_NICK_MAX) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        char c = nick[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}
