#ifndef ROOM_H
#define ROOM_H

/*
 * 聊天室的业务规则：昵称登记、群发、私聊、在线名单。
 *
 * 这一层不碰套接字。网络层告诉它“编号为 id 的成员发来一条消息”，它处理之后
 * 通过 send 回调告诉网络层“把这条消息发给编号为 id 的成员”，至于消息怎样编码、
 * 什么时候真正写进套接字，由网络层决定。因此这一层可以脱离网络单独测试
 * （见 tests/test_room.c）。
 *
 * 成员编号由网络层分配，取值 0 ~ ROOM_CAPACITY - 1，编号相同就是同一个连接。
 * 连接刚建立时成员处于“未登记”状态，发出合法的 NICK 之后才算进入聊天室。
 */

#include "protocol.h"

#include <stdbool.h>

#define ROOM_CAPACITY 32

typedef void room_send_fn(void *ctx, int member_id, const message *msg);

typedef struct {
    bool registered;
    char nick[PROTO_NICK_MAX + 1];
} room_member;

typedef struct {
    room_member members[ROOM_CAPACITY];
    room_send_fn *send; /* 由网络层提供 */
    void *ctx;          /* 原样传回给 send */
} chat_room;

void room_init(chat_room *room, room_send_fn *send, void *ctx);

/* 处理成员 id 发来的一条消息。 */
void room_handle(chat_room *room, int id, const message *msg);

/* 成员 id 的连接已经关闭。若它已登记，通知其他人它离开了。 */
void room_leave(chat_room *room, int id);

#endif
