#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 错误示范：把帧头定义成结构体，再整个写到线路上。
 * 先用 0xAA 填满，好让编译器插入的填充字节显出来。 */
struct frame_header {
    uint8_t type;
    uint16_t body_len;
};

int main(void) {
    struct frame_header h;
    memset(&h, 0xAA, sizeof h);
    h.type = 0x02;
    h.body_len = 3;

    const unsigned char *p = (const unsigned char *)&h;
    printf("sizeof(struct frame_header) = %d\n", (int)sizeof h);
    for (size_t i = 0; i < sizeof h; i++) {
        printf("%02x%s", p[i], i + 1 < sizeof h ? " " : "\n");
    }
    return 0;
}
