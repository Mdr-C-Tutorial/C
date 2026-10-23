#ifndef PROTOCOL_H
#define PROTOCOL_H

/*
 * Mdr Chat Room 的线路格式（wire format）：客户端和服务器之间传送的字节。
 *
 * TCP 只保证字节按顺序到达，不保留“一次 send 发了多少”的边界：一条消息可能
 * 要好几次 recv 才收全，好几条消息也可能一次 recv 就全部收到。所以每条消息在
 * 开头写明自己的长度，接收方据此把字节流重新切成一条条消息。编码后的一条消息
 * 称为一帧（frame）：
 *
 *   偏移  长度  字段
 *   0     1     type      消息类型，取值见 msg_type
 *   1     2     body_len  帧体长度，大端序（网络字节序），1 ~ PROTO_BODY_MAX
 *   3     1     name_len  name 的字节数，0 ~ PROTO_NICK_MAX
 *   4     n     name      昵称，n = name_len
 *   4+n   m     text      正文，占满帧体的剩余部分，m = body_len - 1 - n
 *
 * 前 3 个字节是帧头，其后 body_len 个字节是帧体。name 和 text 不带结尾的
 * '\0'，也不得含控制字符（0x00 ~ 0x1F 和 0x7F）：服务器会把 text 转发到
 * 别人的终端上，混进去的 ESC 序列能清屏、改窗口标题，'\n' 能伪造出一行
 * “系统通知”。其余字节原样传送，所以 UTF-8 编码的中文可以直接发。
 *
 * 例：SAY "hi" 编码为 02 00 03 00 68 69。
 */

#include <stdbool.h>
#include <stddef.h>

#define PROTO_NICK_MAX 16
#define PROTO_TEXT_MAX 1024
#define PROTO_HEADER_SIZE 3
#define PROTO_BODY_MAX (1 + PROTO_NICK_MAX + PROTO_TEXT_MAX)
#define PROTO_FRAME_MAX (PROTO_HEADER_SIZE + PROTO_BODY_MAX)

/*
 * 客户端发出的类型小于 0x80，服务器发出的类型最高位为 1。请求和它引起的
 * 消息只差最高位：客户端发 SAY (0x02)，所有人收到 CHAT (0x82)。
 * 收到不该由对方发出的类型（例如客户端发来 CHAT），由上层当作错误处理。
 */
typedef enum {
    MSG_NICK = 0x01,    /* 登记或更改昵称。name：昵称 */
    MSG_SAY = 0x02,     /* 群发。text：内容 */
    MSG_WHISPER = 0x03, /* 私聊。name：接收者；text：内容 */
    MSG_LIST = 0x04,    /* 查询在线名单。不用字段 */

    MSG_WELCOME = 0x81, /* 登记成功。name：昵称；text：欢迎词 */
    MSG_CHAT = 0x82,    /* 群发的消息。name：发言者；text：内容 */
    MSG_PRIVATE = 0x83, /* 私聊的消息。name：发送者；text：内容 */
    MSG_USERS = 0x84,   /* 在线名单。text：以空格分隔的昵称 */
    MSG_NOTICE = 0x85,  /* 系统通知。text：例如 "alice joined" */
    MSG_ERROR = 0x86    /* 请求失败。text：原因 */
} msg_type;

/* 解码后的消息。两个字段都以 '\0' 结尾，不用的字段为空串。 */
typedef struct {
    msg_type type;
    char name[PROTO_NICK_MAX + 1];
    char text[PROTO_TEXT_MAX + 1];
} message;

typedef enum {
    PROTO_OK,         /* 解出了一条完整的消息 */
    PROTO_INCOMPLETE, /* 字节还不够一帧，等收到更多数据再试 */
    PROTO_BAD_TYPE,   /* 未知的消息类型 */
    PROTO_BAD_LENGTH, /* body_len 超出范围 */
    PROTO_BAD_FIELD   /* name_len 与帧体不符，或字段里有控制字符 */
} proto_status;

/*
 * 接收缓冲区：调用方把 recv 读到的字节追加到 data[len] 之后，再用
 * frame_buffer_next 逐帧取出。容量是一帧的上限，这就够用：缓冲区满时，
 * 开头那一帧一定已经完整（帧头里的长度不会超过 PROTO_FRAME_MAX），
 * 取走它就腾出了空间。
 */
typedef struct {
    unsigned char data[PROTO_FRAME_MAX];
    size_t len;
} frame_buffer;

/* 填写一条消息。NULL 当作空串；超出字段上限的部分被截断，
 * 在意长度的调用方应先自行检查。 */
void message_set(message *msg, msg_type type, const char *name,
                 const char *text);

/* 把 msg 编码成一帧写入 out，返回帧长；放不下或字段过长时返回 0。 */
size_t proto_encode(const message *msg, unsigned char *out, size_t cap);

/* 解码 data 开头的一帧（data 里可以有好几帧，只看第一帧）。返回 PROTO_OK
 * 时消息写入 *msg、帧长写入 *frame_len；其他返回值不修改这两个对象。 */
proto_status proto_decode(const unsigned char *data, size_t len, message *msg,
                          size_t *frame_len);

/* 从缓冲区开头取出一帧。返回 PROTO_OK 时这一帧已从缓冲区移除；返回错误时
 * 缓冲区保持原样，此后的字节无法再解析，调用方应断开连接。 */
proto_status frame_buffer_next(frame_buffer *buf, message *msg);

const char *proto_status_text(proto_status status);
const char *msg_type_name(msg_type type);

/* 合法的昵称：1 ~ PROTO_NICK_MAX 个 ASCII 字母、数字、'_' 或 '-'。 */
bool nick_is_valid(const char *nick);

#endif
