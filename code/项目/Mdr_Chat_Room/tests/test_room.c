#include "room.h"

#include "check.h"

#include <stdbool.h>
#include <string.h>

/* 假的网络层：把聊天室交出来的消息按顺序记下来，供断言检查。 */
#define OUTBOX_MAX 32

typedef struct {
    int to[OUTBOX_MAX];
    message msgs[OUTBOX_MAX];
    int count;
} outbox;

static void record(void *ctx, int member_id, const message *msg) {
    outbox *box = ctx;
    if (box->count < OUTBOX_MAX) {
        box->to[box->count] = member_id;
        box->msgs[box->count] = *msg;
        box->count++;
    }
}

/* 统计发给 to、类型为 type 的消息里，name 和 text 都匹配的有几条。
 * name 或 text 为 NULL 时不比较该字段。 */
static int count_sent(const outbox *box, int to, msg_type type,
                      const char *name, const char *text) {
    int count = 0;
    for (int i = 0; i < box->count; i++) {
        const message *m = &box->msgs[i];
        if (box->to[i] == to && m->type == type &&
            (name == NULL || strcmp(m->name, name) == 0) &&
            (text == NULL || strcmp(m->text, text) == 0)) {
            count++;
        }
    }
    return count;
}

static void handle(chat_room *room, int id, msg_type type, const char *name,
                   const char *text) {
    message msg;
    message_set(&msg, type, name, text);
    room_handle(room, id, &msg);
}

/* 所有测试共用一个聊天室和一个记录本，放在静态存储区：记录本里有 32 条
 * 消息，每条一千多字节。 */
static outbox box;
static chat_room room;

/* 每个测试都从空聊天室和空记录开始 */
static void reset(void) {
    memset(&box, 0, sizeof box);
    room_init(&room, record, &box);
}

/* 让 id 号成员登记为 nick，然后清空记录，只留下测试关心的消息 */
static void join(int id, const char *nick) {
    handle(&room, id, MSG_NICK, nick, NULL);
    box.count = 0;
}

static void test_register(void) {
    reset();
    handle(&room, 0, MSG_NICK, "alice", NULL);
    CHECK(box.count == 1);
    CHECK(count_sent(&box, 0, MSG_WELCOME, "alice",
                     "welcome to Mdr Chat Room, alice (1 online)") == 1);

    box.count = 0;
    handle(&room, 1, MSG_NICK, "bob", NULL);
    CHECK(count_sent(&box, 1, MSG_WELCOME, "bob", NULL) == 1);
    CHECK(count_sent(&box, 0, MSG_NOTICE, "", "bob joined") == 1);
    CHECK(box.count == 2); /* bob 自己收到欢迎词，而不是 "bob joined" */
}

static void test_bad_nicknames(void) {
    reset();
    join(0, "alice");
    handle(&room, 1, MSG_NICK, "alice", NULL);
    CHECK(count_sent(&box, 1, MSG_ERROR, "",
                     "nickname is already in use: alice") == 1);
    handle(&room, 1, MSG_NICK, "a b", NULL);
    CHECK(count_sent(&box, 1, MSG_ERROR, NULL, NULL) == 2);
    CHECK(box.count == 2); /* 失败的登记不会通知别人 */

    /* 两次失败之后 1 号仍未登记，发言被拒绝 */
    box.count = 0;
    handle(&room, 1, MSG_SAY, NULL, "hello?");
    CHECK(count_sent(&box, 1, MSG_ERROR, "", "register a nickname first") ==
          1);
    CHECK(box.count == 1);
}

static void test_say_reaches_registered_members(void) {
    reset();
    join(0, "alice");
    join(1, "bob");
    /* 2 号已经连上，但还没登记 */
    handle(&room, 0, MSG_SAY, NULL, "hello");
    CHECK(count_sent(&box, 0, MSG_CHAT, "alice", "hello") == 1);
    CHECK(count_sent(&box, 1, MSG_CHAT, "alice", "hello") == 1);
    CHECK(box.count == 2);
}

static void test_whisper(void) {
    reset();
    join(0, "alice");
    join(1, "bob");
    join(2, "carol");
    handle(&room, 0, MSG_WHISPER, "carol", "psst");
    CHECK(count_sent(&box, 2, MSG_PRIVATE, "alice", "psst") == 1);
    CHECK(box.count == 1); /* 只有接收者收到 */

    box.count = 0;
    handle(&room, 0, MSG_WHISPER, "dave", "hi");
    CHECK(count_sent(&box, 0, MSG_ERROR, "", "no such user: dave") == 1);
    CHECK(box.count == 1);
}

static void test_list_in_member_order(void) {
    reset();
    join(2, "carol");
    join(0, "alice");
    join(5, "bob");
    handle(&room, 0, MSG_LIST, NULL, NULL);
    CHECK(count_sent(&box, 0, MSG_USERS, "", "alice carol bob") == 1);
}

static void test_rename(void) {
    reset();
    join(0, "alice");
    join(1, "bob");
    handle(&room, 1, MSG_NICK, "robert", NULL);
    CHECK(count_sent(&box, 0, MSG_NOTICE, "", "bob is now known as robert") ==
          1);
    CHECK(count_sent(&box, 1, MSG_NOTICE, "", "bob is now known as robert") ==
          1);

    /* 旧名字空出来了，别人可以用 */
    box.count = 0;
    handle(&room, 2, MSG_NICK, "bob", NULL);
    CHECK(count_sent(&box, 2, MSG_WELCOME, "bob", NULL) == 1);

    /* 改成自己现在的名字：什么也不发 */
    box.count = 0;
    handle(&room, 0, MSG_NICK, "alice", NULL);
    CHECK(box.count == 0);
}

static void test_leave(void) {
    reset();
    join(0, "alice");
    join(1, "bob");
    room_leave(&room, 0);
    CHECK(count_sent(&box, 1, MSG_NOTICE, "", "alice left") == 1);
    CHECK(box.count == 1);

    /* 离开后昵称被释放；未登记的连接离开时没人需要知道 */
    box.count = 0;
    handle(&room, 3, MSG_NICK, "alice", NULL);
    CHECK(count_sent(&box, 3, MSG_WELCOME, "alice", NULL) == 1);
    box.count = 0;
    room_leave(&room, 4);
    CHECK(box.count == 0);
}

static void test_server_only_type_is_rejected(void) {
    reset();
    join(0, "alice");
    handle(&room, 0, MSG_CHAT, "alice", "spoofed");
    CHECK(count_sent(&box, 0, MSG_ERROR, "", "unexpected message type") == 1);
    CHECK(box.count == 1);
}

int main(void) {
    test_register();
    test_bad_nicknames();
    test_say_reaches_registered_members();
    test_whisper();
    test_list_in_member_order();
    test_rename();
    test_leave();
    test_server_only_type_is_rejected();
    return check_report("test_room");
}
