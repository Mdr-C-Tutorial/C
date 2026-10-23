#include "room.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* USERS 回复把所有在线昵称用空格连成一条 text，所以容量受 text 上限约束。
 * 改大 ROOM_CAPACITY 时，这条断言会在编译期提醒你同时改大 PROTO_TEXT_MAX。
 * _Static_assert 是 C11 引入的。 */
_Static_assert(ROOM_CAPACITY * (PROTO_NICK_MAX + 1) <= PROTO_TEXT_MAX + 1,
               "a USERS reply must fit in one message");

void room_init(chat_room *room, room_send_fn *send, void *ctx) {
    *room = (chat_room){.send = send, .ctx = ctx};
}

static void send_one(chat_room *room, int to, msg_type type, const char *name,
                     const char *text) {
    message msg;
    message_set(&msg, type, name, text);
    room->send(room->ctx, to, &msg);
}

/* 发给除 except 以外的所有已登记成员（except 为 -1 表示不排除任何人）。
 * 未登记的连接还不算进入聊天室，收不到群发。 */
static void send_all(chat_room *room, int except, msg_type type,
                     const char *name, const char *text) {
    message msg;
    message_set(&msg, type, name, text);
    for (int id = 0; id < ROOM_CAPACITY; id++) {
        if (id != except && room->members[id].registered) {
            room->send(room->ctx, id, &msg);
        }
    }
}

static int find_member(const chat_room *room, const char *nick) {
    for (int id = 0; id < ROOM_CAPACITY; id++) {
        const room_member *m = &room->members[id];
        if (m->registered && strcmp(m->nick, nick) == 0) {
            return id;
        }
    }
    return -1;
}

static int online_count(const chat_room *room) {
    int count = 0;
    for (int id = 0; id < ROOM_CAPACITY; id++) {
        if (room->members[id].registered) {
            count++;
        }
    }
    return count;
}

static void handle_nick(chat_room *room, int id, const char *nick) {
    room_member *self = &room->members[id];
    char text[PROTO_TEXT_MAX + 1];

    if (!nick_is_valid(nick)) {
        send_one(room, id, MSG_ERROR, NULL,
                 "invalid nickname: use 1-16 letters, digits, '_' or '-'");
        return;
    }
    int owner = find_member(room, nick);
    if (owner == id) {
        return; /* 改成自己现在的名字，什么也不用做 */
    }
    if (owner >= 0) {
        snprintf(text, sizeof text, "nickname is already in use: %s", nick);
        send_one(room, id, MSG_ERROR, NULL, text);
        return;
    }

    if (!self->registered) {
        snprintf(self->nick, sizeof self->nick, "%s", nick);
        self->registered = true;
        snprintf(text, sizeof text, "welcome to Mdr Chat Room, %s (%d online)",
                 nick, online_count(room));
        send_one(room, id, MSG_WELCOME, nick, text);
        snprintf(text, sizeof text, "%s joined", nick);
        send_all(room, id, MSG_NOTICE, NULL, text);
        return;
    }
    snprintf(text, sizeof text, "%s is now known as %s", self->nick, nick);
    snprintf(self->nick, sizeof self->nick, "%s", nick);
    send_all(room, -1, MSG_NOTICE, NULL, text);
}

static void handle_whisper(chat_room *room, int id, const message *msg) {
    int target = find_member(room, msg->name);
    if (target < 0) {
        char text[PROTO_TEXT_MAX + 1];
        snprintf(text, sizeof text, "no such user: %s", msg->name);
        send_one(room, id, MSG_ERROR, NULL, text);
        return;
    }
    send_one(room, target, MSG_PRIVATE, room->members[id].nick, msg->text);
}

static void handle_list(chat_room *room, int id) {
    /* 文件开头的静态断言保证所有昵称都放得下 */
    char text[PROTO_TEXT_MAX + 1];
    size_t len = 0;
    for (int i = 0; i < ROOM_CAPACITY; i++) {
        const room_member *m = &room->members[i];
        if (!m->registered) {
            continue;
        }
        if (len > 0) {
            text[len++] = ' ';
        }
        size_t nick_len = strlen(m->nick);
        memcpy(text + len, m->nick, nick_len);
        len += nick_len;
    }
    text[len] = '\0';
    send_one(room, id, MSG_USERS, NULL, text);
}

void room_handle(chat_room *room, int id, const message *msg) {
    assert(id >= 0 && id < ROOM_CAPACITY);
    if (msg->type == MSG_NICK) {
        handle_nick(room, id, msg->name);
        return;
    }
    /* 群发、私聊都要标明“是谁说的”，所以登记昵称之前只能发 NICK */
    if (!room->members[id].registered) {
        send_one(room, id, MSG_ERROR, NULL, "register a nickname first");
        return;
    }
    switch (msg->type) {
    case MSG_SAY:
        send_all(room, -1, MSG_CHAT, room->members[id].nick, msg->text);
        break;
    case MSG_WHISPER:
        handle_whisper(room, id, msg);
        break;
    case MSG_LIST:
        handle_list(room, id);
        break;
    default:
        /* 帧格式正确，但这是服务器才会发出的类型 */
        send_one(room, id, MSG_ERROR, NULL, "unexpected message type");
        break;
    }
}

void room_leave(chat_room *room, int id) {
    assert(id >= 0 && id < ROOM_CAPACITY);
    room_member *self = &room->members[id];
    if (!self->registered) {
        return; /* 没登记过，别人本来就不知道它 */
    }
    char text[PROTO_TEXT_MAX + 1];
    snprintf(text, sizeof text, "%s left", self->nick);
    *self = (room_member){.registered = false};
    send_all(room, -1, MSG_NOTICE, NULL, text);
}
