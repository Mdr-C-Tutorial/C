#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t magic;
    uint32_t pc;
    uint32_t lr;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t bfar;
    uint32_t checksum;
} crash_record_t;

#define CRASH_MAGIC 0x4641554Cu

static crash_record_t g_sim_noinit;

/* TODO(1): 计算除 checksum 字段外其余 6 个 32 位字段与 0xA5A55A5Au 的异或校验值 */
static uint32_t crash_checksum(const crash_record_t *rec) {
    (void)rec;
    return 0u;
}

/* TODO(2): 把故障现场写入 g_sim_noinit，并填入 magic 与 checksum */
void crash_record_save(uint32_t pc, uint32_t lr, uint32_t cfsr,
                       uint32_t hfsr, uint32_t bfar) {
    (void)pc;
    (void)lr;
    (void)cfsr;
    (void)hfsr;
    (void)bfar;
}

/* TODO(3): 校验 g_sim_noinit；有效时复制到 *out 并返回 1，否则返回 0 */
int crash_record_load(crash_record_t *out) {
    (void)out;
    (void)crash_checksum(&g_sim_noinit);
    return 0;
}

/* TODO(4): 清除 g_sim_noinit 中的有效标记，防止下次正常复位时重复打印 */
void crash_record_clear(void) {
}

int main(void) {
    crash_record_t out;

    /* 模拟上电：RAM 中是随机脏数据 */
    memset(&g_sim_noinit, 0xCC, sizeof g_sim_noinit);
    printf("cold boot valid = %d\n", crash_record_load(&out));

    /* 模拟发生精确总线错误并保存记录 */
    crash_record_save(0x08000110u, 0x08000159u, 0x00008200u,
                      0x40000000u, 0xCCCCCCCCu);
    if (crash_record_load(&out)) {
        printf("reboot 1: PC=0x%08X LR=0x%08X CFSR=0x%08X BFAR=0x%08X\n",
               out.pc, out.lr, out.cfsr, out.bfar);
        crash_record_clear();
    }

    /* 清除后再次复位，不应重复打印 */
    printf("reboot 2 valid = %d\n", crash_record_load(&out));
    return 0;
}
