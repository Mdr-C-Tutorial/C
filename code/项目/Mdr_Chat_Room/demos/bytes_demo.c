#include "protocol.h"

#include <stdio.h>
#include <string.h>

/* 打印几种消息编码后的字节，与正文的字节表对照。
 * 编译：cc -std=c11 -I../include bytes_demo.c ../src/protocol.c -o bytes_demo */
static void show(const char *label, msg_type type, const char *name,
                 const char *text) {
    message msg;
    unsigned char frame[PROTO_FRAME_MAX];
    message_set(&msg, type, name, text);
    size_t len = proto_encode(&msg, frame, sizeof frame);
    printf("%-24s (%3d 字节):", label, (int)len);
    for (size_t i = 0; i < len && i < 12; i++) {
        printf(" %02x", frame[i]);
    }
    printf(len > 12 ? " ...\n" : "\n");
}

int main(void) {
    char text[PROTO_TEXT_MAX + 1];
    memset(text, 'x', 300);
    text[300] = '\0';

    show("NICK alice", MSG_NICK, "alice", NULL);
    show("SAY \"hi bob\"", MSG_SAY, NULL, "hi bob");
    show("WHISPER bob \"hi\"", MSG_WHISPER, "bob", "hi");
    show("USERS \"alice bob\"", MSG_USERS, NULL, "alice bob");
    show("SAY 300 个 x", MSG_SAY, NULL, text);
    return 0;
}
