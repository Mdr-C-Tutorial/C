/* cbpf_run.c：47105 的参考实现。一个只支持 ldh、ldb、jeq、jgt、ret 的
 * 经典 BPF 解释器，用它执行内核文档中的 IPv4 TCP 过滤器。
 *
 * 编译：gcc -std=c17 -Wall -Wextra cbpf_run.c -o cbpf_run
 */
#include <stdint.h>
#include <stdio.h>

struct insn {
    uint16_t code;
    uint8_t jt;
    uint8_t jf;
    uint32_t k;
};

enum {
    OP_LDH = 0x28,
    OP_LDB = 0x30,
    OP_JEQ = 0x15,
    OP_JGT = 0x25,
    OP_RET = 0x06,
};

static uint32_t run(const struct insn *prog, size_t n, const uint8_t *pkt,
                    size_t len) {
    uint32_t a = 0;
    size_t pc = 0;

    while (pc < n) {
        const struct insn *i = &prog[pc];
        switch (i->code) {
        case OP_LDH:
            if ((size_t)i->k + 2 > len) {
                printf("  ldh [%u] out of bounds, dropping\n", i->k);
                return 0;
            }
            a = ((uint32_t)pkt[i->k] << 8) | pkt[i->k + 1];
            pc++;
            break;
        case OP_LDB:
            if ((size_t)i->k + 1 > len) {
                printf("  ldb [%u] out of bounds, dropping\n", i->k);
                return 0;
            }
            a = pkt[i->k];
            pc++;
            break;
        case OP_JEQ:
            pc += 1 + ((a == i->k) ? (size_t)i->jt : (size_t)i->jf);
            break;
        case OP_JGT:
            pc += 1 + ((a > i->k) ? (size_t)i->jt : (size_t)i->jf);
            break;
        case OP_RET:
            return i->k;
        default:
            printf("  unknown opcode 0x%02x\n", i->code);
            return 0;
        }
    }
    return 0;
}

/* ldh [12]; jeq #0x800; ldb [23]; jeq #6; ret #-1; ret #0 */
static const struct insn ipv4_tcp[] = {
    {OP_LDH, 0, 0, 12}, {OP_JEQ, 0, 3, 0x800}, {OP_LDB, 0, 0, 23},
    {OP_JEQ, 0, 1, 6},  {OP_RET, 0, 0, 0xffffffffu}, {OP_RET, 0, 0, 0},
};

/* 与上面相同，只把第二条比较换成 jgt：以太网类型大于 0x800 就丢弃。 */
static const struct insn ipv4_gt[] = {
    {OP_LDH, 0, 0, 12}, {OP_JGT, 3, 0, 0x800}, {OP_LDB, 0, 0, 23},
    {OP_JEQ, 0, 1, 6},  {OP_RET, 0, 0, 0xffffffffu}, {OP_RET, 0, 0, 0},
};

static void test(const char *name, uint16_t ethertype, uint8_t proto) {
    uint8_t frame[64] = {0};

    frame[12] = (uint8_t)(ethertype >> 8);
    frame[13] = (uint8_t)(ethertype & 0xff);
    frame[23] = proto;
    printf("%-9s ret = 0x%08x\n", name,
           run(ipv4_tcp, sizeof ipv4_tcp / sizeof ipv4_tcp[0], frame,
               sizeof frame));
}

int main(void) {
    printf("filter: ldh[12] == 0x800 && ldb[23] == 6\n");
    test("ARP", 0x0806, 0);
    test("IPv4 UDP", 0x0800, 17);
    test("IPv4 TCP", 0x0800, 6);
    test("IPv6", 0x86dd, 0);

    printf("\nsame filter with jgt instead of the first jeq\n");
    uint8_t frame[64] = {0};
    frame[12] = 0x08;
    frame[13] = 0x06;
    frame[23] = 6;
    printf("%-9s ret = 0x%08x\n", "ARP",
           run(ipv4_gt, sizeof ipv4_gt / sizeof ipv4_gt[0], frame,
               sizeof frame));
    frame[13] = 0x00;
    printf("%-9s ret = 0x%08x\n", "IPv4 TCP",
           run(ipv4_gt, sizeof ipv4_gt / sizeof ipv4_gt[0], frame,
               sizeof frame));
    return 0;
}