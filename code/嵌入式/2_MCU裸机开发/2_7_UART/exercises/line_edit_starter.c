#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LINE_CAP 64u

typedef struct {
    char buf[LINE_CAP];
    size_t len;
} line_editor;

/*
 * 向行编辑器喂入一个接收到的字符 ch，并把需要回显到终端的字节写入 echo_out。
 * 返回值：
 *   1 表示收到回车（'\r' 或 '\n'），此时 buf 中是以 '\0' 结尾的一行；
 *   0 表示仍在编辑中。
 */
int line_editor_feed(line_editor *ed, char ch, char *echo_out, size_t echo_cap) {
    /* TODO: 处理回车（'\r' / '\n'）、退格（'\b' 或 0x7F）与普通可打印字符 */
    (void)ed;
    (void)ch;
    if (echo_cap > 0) {
        echo_out[0] = '\0';
    }
    return 0;
}

int main(void) {
    const char *stream = "led o\x7fn\rstatus\r";
    line_editor ed = { .len = 0 };
    char echo_buf[16];

    for (const char *p = stream; *p != '\0'; ++p) {
        if (line_editor_feed(&ed, *p, echo_buf, sizeof echo_buf)) {
            printf("LINE: \"%s\"\n", ed.buf);
            ed.len = 0;
        }
    }
    return 0;
}
