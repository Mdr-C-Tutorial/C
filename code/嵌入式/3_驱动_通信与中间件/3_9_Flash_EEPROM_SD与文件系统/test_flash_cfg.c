#include "flash_cfg.h"
#include <stdio.h>
#include <string.h>

/* 模拟一页（1 KiB）NOR Flash：擦除置 0xFF，编程只能将 1 改为 0 */
static uint8_t sim_flash[CFG_PAGE_SIZE];
static size_t sim_fail_after_halfwords = 0u;
static size_t sim_halfword_count = 0u;
static unsigned sim_erase_count = 0u;

static bool sim_erase_page(uint32_t page_offset) {
    if (page_offset != 0u) {
        return false;
    }
    memset(sim_flash, 0xFF, sizeof(sim_flash));
    ++sim_erase_count;
    return true;
}

static bool sim_program_halfword(uint32_t offset, uint16_t halfword) {
    if ((offset & 1u) != 0u || offset + 2u > sizeof(sim_flash)) {
        return false;
    }
    if (sim_fail_after_halfwords > 0u &&
        sim_halfword_count >= sim_fail_after_halfwords) {
        return false; /* 模拟写到一半断电 */
    }
    sim_halfword_count++;

    uint16_t old_val = (uint16_t)sim_flash[offset] |
                       ((uint16_t)sim_flash[offset + 1u] << 8);
    /* NOR Flash 物理规律：新值只能把已有的 1 变成 0，不能把 0 变成 1 */
    uint16_t new_val = (uint16_t)(old_val & halfword);
    sim_flash[offset] = (uint8_t)(new_val & 0xFFu);
    sim_flash[offset + 1u] = (uint8_t)(new_val >> 8);

    /* 原半字不是 0xFFFF 时再次编程，硬件置 PGERR */
    if (old_val != 0xFFFFu) {
        return false;
    }
    return true;
}

static void sim_read_bytes(uint32_t offset, void *buf, size_t len) {
    memcpy(buf, &sim_flash[offset], len);
}

static const flash_ops_t sim_ops = {
    .erase_page = sim_erase_page,
    .program_halfword = sim_program_halfword,
    .read_bytes = sim_read_bytes,
};

int main(void) {
    memset(sim_flash, 0xFF, sizeof(sim_flash));

    /* 1. 演示不擦除直接改写：0x5678 改写为 0x1234 */
    sim_erase_page(0u);
    bool ok1 = sim_program_halfword(0u, 0x5678u);
    bool ok2 = sim_program_halfword(0u, 0x1234u);
    uint16_t read_hw = (uint16_t)sim_flash[0] | ((uint16_t)sim_flash[1] << 8);
    printf("[overwrite without erase] first=0x5678(ok=%d) second=0x1234(ok=%d) -> read=0x%04X\n",
           ok1, ok2, read_hw);

    /* 2. 追加三条记录 */
    memset(sim_flash, 0xFF, sizeof(sim_flash));
    cfg_record_t cfg = {
        .node_addr = 1u,
        .report_sec = 10u,
        .calib_offset = -15,
        .calib_gain = 1002,
    };
    (void)flash_cfg_save(&sim_ops, &cfg);
    printf("[save #1] seq=%u addr=%u offset=%d crc=0x%08X\n",
           (unsigned)cfg.seq, cfg.node_addr, (int)cfg.calib_offset,
           (unsigned)cfg.crc32);

    cfg.node_addr = 2u;
    cfg.calib_offset = 5;
    (void)flash_cfg_save(&sim_ops, &cfg);
    printf("[save #2] seq=%u addr=%u offset=%d crc=0x%08X\n",
           (unsigned)cfg.seq, cfg.node_addr, (int)cfg.calib_offset,
           (unsigned)cfg.crc32);

    cfg.node_addr = 3u;
    cfg.calib_offset = 20;
    (void)flash_cfg_save(&sim_ops, &cfg);
    printf("[save #3] seq=%u addr=%u offset=%d crc=0x%08X\n",
           (unsigned)cfg.seq, cfg.node_addr, (int)cfg.calib_offset,
           (unsigned)cfg.crc32);

    /* 3. 第 4 次写入在第 6 个半字后断电 */
    sim_halfword_count = 0u;
    sim_fail_after_halfwords = 6u;
    cfg.node_addr = 99u;
    cfg.calib_offset = 999;
    bool save4_ok = flash_cfg_save(&sim_ops, &cfg);
    sim_fail_after_halfwords = 0u;

    cfg_record_t loaded;
    bool load_ok = flash_cfg_load(&sim_ops, &loaded);
    printf("[power loss on #4] save_ok=%d -> recovered: load_ok=%d seq=%u addr=%u offset=%d\n",
           save4_ok, load_ok, (unsigned)loaded.seq, loaded.node_addr,
           (int)loaded.calib_offset);

    /* 4. 从干净的一页开始连写 70 次，记录整页擦除发生在第几次，检查序号是否连续 */
    sim_erase_page(0u);
    unsigned erases_before = sim_erase_count;
    unsigned erases_in_loop = 0u;
    for (unsigned n = 0u; n < 70u; ++n) {
        cfg_record_t rec = {0};
        (void)flash_cfg_load(&sim_ops, &rec);
        uint32_t before = rec.seq;
        rec.node_addr = (uint16_t)(n + 1u);
        rec.report_sec = 10u;
        rec.calib_gain = 1000;
        if (!flash_cfg_save(&sim_ops, &rec)) {
            printf("[append] save failed at n=%u\n", n);
            break;
        }
        if (sim_erase_count != erases_before) {
            erases_before = sim_erase_count;
            ++erases_in_loop;
            printf("[append] save #%2u: page erased, seq %u -> %u, slot %u\n",
                   n + 1u, (unsigned)before, (unsigned)rec.seq,
                   (unsigned)((rec.seq - 1u) % CFG_SLOT_COUNT));
        }
        if (rec.seq != before + 1u) {
            printf("[append] seq gap at save #%u: %u -> %u\n", n + 1u,
                   (unsigned)before, (unsigned)rec.seq);
        }
    }
    (void)flash_cfg_load(&sim_ops, &loaded);
    printf("[append] after 70 saves: seq=%u addr=%u, page erases=%u\n",
           (unsigned)loaded.seq, loaded.node_addr, erases_in_loop);
    return 0;
}