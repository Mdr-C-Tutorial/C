/* mini_vm.c：只实现 MOV、ADD、MUL、条件跳转与 EXIT 的袖珍 BPF 解释器。
 * 指令字节取自 llvm-objdump -d calc.bpf.o 的输出（calc 用 -O2 编译）。
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define REG_COUNT 11
#define STACK_SIZE 512

enum bpf_opcode {
    OP_MOV_IMM = 0xb7,
    OP_MOV_REG = 0xbf,
    OP_ADD_IMM = 0x07,
    OP_ADD_REG = 0x0f,
    OP_MUL_IMM = 0x27,
    OP_MUL_REG = 0x2f,
    OP_JA = 0x05,
    OP_JSGT_IMM = 0x65,
    OP_JSGT_REG = 0x6d,
    OP_EXIT = 0x95,
};

struct insn {
    uint8_t opcode;
    uint8_t regs;
    int16_t off;
    int32_t imm;
};

static struct insn decode(const uint8_t *p) {
    struct insn in;

    in.opcode = p[0];
    in.regs = p[1];
    in.off = (int16_t)((uint16_t)p[2] | ((uint16_t)p[3] << 8));
    in.imm = (int32_t)((uint32_t)p[4] | ((uint32_t)p[5] << 8) |
                       ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24));
    return in;
}

static void dump(size_t pc, const struct insn *in, unsigned dst, unsigned src,
                 const uint64_t *regs) {
    printf("  %2d: opcode 0x%02x dst r%u src r%u off %+d imm %d\n", (int)pc,
           in->opcode, dst, src, (int)in->off, (int)in->imm);
    printf("      r0 = %llu  r1 = %llu\n", (unsigned long long)regs[0],
           (unsigned long long)regs[1]);
}

static int run(const uint8_t *code, size_t count, uint64_t arg, int trace,
               uint64_t *result) {
    uint64_t regs[REG_COUNT] = {0};
    size_t pc = 0;

    regs[1] = arg;
    regs[10] = STACK_SIZE;
    while (pc < count) {
        struct insn in = decode(code + pc * 8);
        unsigned dst = in.regs & 0x0f;
        unsigned src = in.regs >> 4;
        size_t here = pc;

        switch (in.opcode) {
        case OP_MOV_IMM:
            regs[dst] = (uint64_t)(int64_t)in.imm;
            pc++;
            break;
        case OP_MOV_REG:
            regs[dst] = regs[src];
            pc++;
            break;
        case OP_ADD_IMM:
            regs[dst] += (uint64_t)(int64_t)in.imm;
            pc++;
            break;
        case OP_ADD_REG:
            regs[dst] += regs[src];
            pc++;
            break;
        case OP_MUL_IMM:
            regs[dst] *= (uint64_t)(int64_t)in.imm;
            pc++;
            break;
        case OP_MUL_REG:
            regs[dst] *= regs[src];
            pc++;
            break;
        case OP_JA:
            pc = (size_t)((int64_t)pc + 1 + in.off);
            break;
        case OP_JSGT_IMM:
            if ((int64_t)regs[dst] > (int64_t)in.imm) {
                pc = (size_t)((int64_t)pc + 1 + in.off);
            } else {
                pc++;
            }
            break;
        case OP_JSGT_REG:
            if ((int64_t)regs[dst] > (int64_t)regs[src]) {
                pc = (size_t)((int64_t)pc + 1 + in.off);
            } else {
                pc++;
            }
            break;
        case OP_EXIT:
            *result = regs[0];
            if (trace) {
                dump(here, &in, dst, src, regs);
            }
            return 0;
        default:
            printf("不认识的 opcode 0x%02x，停在指令 %d\n", in.opcode, (int)pc);
            return -1;
        }
        if (trace) {
            dump(here, &in, dst, src, regs);
        }
    }
    return -1;
}

static long calc_c(long x) { return x > 10 ? x * 3 : x + 1; }

int main(void) {
    static const uint8_t code[] = {
        0xbf, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* r0 = r1 */
        0x27, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, /* r0 *= 3 */
        0x65, 0x01, 0x02, 0x00, 0x0a, 0x00, 0x00, 0x00, /* if r1 s> 10 */
        0x07, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, /* r1 += 1 */
        0xbf, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* r0 = r1 */
        0x95, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* exit */
    };
    static const long samples[] = {0, 1, 5, 10, 11, 20, 100};
    size_t count = sizeof(code) / 8;

    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
        uint64_t got = 0;

        if (run(code, count, (uint64_t)samples[i], 0, &got) != 0) {
            return 1;
        }
        printf("x = %3ld   BPF: %3llu   C: %3ld%s\n", samples[i],
               (unsigned long long)got, calc_c(samples[i]),
               (long)got == calc_c(samples[i]) ? "" : "   <-- 不一致");
    }

    printf("\nx = 20 的执行过程：\n");
    uint64_t got = 0;
    if (run(code, count, 20, 1, &got) != 0) {
        return 1;
    }
    printf("\n  结果 %llu\n", (unsigned long long)got);
    return 0;
}
