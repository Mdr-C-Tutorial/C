#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 模拟 lwIP 的 pbuf 节点结构（与 lwip/pbuf.h 核心字段一致） */
struct pbuf {
    struct pbuf *next;
    const void *payload;
    uint16_t tot_len;
    uint16_t len;
};

/*
 * TODO：按顺序遍历 pbuf 链表，将各节点的 payload 复制到连续缓冲区 dst 中，
 * 最多复制 max_len 字节，返回实际复制的总字节数。
 */
size_t pbuf_copy_out(const struct pbuf *p, void *dst, size_t max_len) {
    (void)p;
    (void)dst;
    (void)max_len;
    return 0u;
}

int main(void) {
    const char part1[] = "GET /index";
    const char part2[] = ".html HTTP";
    const char part3[] = "/1.1\r\n";

    struct pbuf b3 = { NULL, part3, 6u, 6u };
    struct pbuf b2 = { &b3, part2, 16u, 10u };
    struct pbuf b1 = { &b2, part1, 26u, 10u };

    char out[32];
    memset(out, 0, sizeof(out));
    size_t n1 = pbuf_copy_out(&b1, out, sizeof(out) - 1u);
    out[n1] = '\0';
    printf("Full copy (%u bytes): %s", (unsigned)n1, out);

    memset(out, 0, sizeof(out));
    size_t n2 = pbuf_copy_out(&b1, out, 15u);
    out[n2] = '\0';
    printf("Partial copy (%u bytes): %s\n", (unsigned)n2, out);
    return 0;
}
