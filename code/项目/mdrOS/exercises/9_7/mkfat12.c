/* 生成 9.7 习题用的 FAT12 镜像 fat12.img（6144 字节，12 个扇区）。
 *
 * 布局：每扇区 512 字节，每簇 1 扇区，保留扇区 1 个，FAT 表 1 份、每份 1 扇区，
 * 根目录 16 项、占 1 个扇区，数据区从第 3 个扇区开始，簇号从 2 起。
 *
 * 两个文件：
 *   HELLO.TXT   700 字节，簇链 2 -> 4
 *   SYSTEM.BIN 1200 字节，簇链 5 -> 7 -> 8
 * 簇 3 与簇 6 的内容看起来像文件开头，但不在任何簇链上。
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SECTOR 512
#define TOTAL_SECTORS 12
#define RESERVED 1
#define NFAT 1
#define FAT_SECTORS 1
#define ROOT_ENTRIES 16
#define ROOT_SECTORS 1
#define DATA_START (RESERVED + NFAT * FAT_SECTORS + ROOT_SECTORS)

static uint8_t volume[TOTAL_SECTORS * SECTOR];

static void put16(uint8_t *p, unsigned v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

/* FAT12 的一项占 12 位：偶数项在字节 low 12 位，奇数项在 high 12 位 */
static void fat12_set(uint8_t *fat, unsigned index, unsigned value) {
    unsigned off = index * 3 / 2;
    if (index & 1) {
        fat[off] = (uint8_t)((fat[off] & 0x0F) | ((value << 4) & 0xF0));
        fat[off + 1] = (uint8_t)(value >> 4);
    } else {
        fat[off] = (uint8_t)(value & 0xFF);
        fat[off + 1] = (uint8_t)((fat[off + 1] & 0xF0) | ((value >> 8) & 0x0F));
    }
}

static void dirent(uint8_t *p, const char *name, const char *ext, uint8_t attr,
                   unsigned cluster, uint32_t size) {
    memset(p, 0, 32);
    memset(p, ' ', 11); /* 8.3 名字不足 8 或 3 个字符时用空格补齐 */
    memcpy(p, name, strlen(name));
    memcpy(p + 8, ext, strlen(ext));
    p[11] = attr;
    put16(p + 14, 0x6000); /* 12:00:00 */
    put16(p + 16, 0x5B2F); /* 2025-09-15 */
    put16(p + 18, 0x5B2F);
    put16(p + 22, 0x6000);
    put16(p + 24, 0x5B2F);
    put16(p + 26, cluster);
    put32(p + 28, size);
}

static uint8_t *cluster_ptr(unsigned cluster) {
    return volume + (DATA_START + (cluster - 2)) * SECTOR;
}

int main(void) {
    uint8_t *boot = volume;
    uint8_t *fat = volume + RESERVED * SECTOR;
    uint8_t *root = volume + (RESERVED + NFAT * FAT_SECTORS) * SECTOR;
    static uint8_t hello[700], system[1200];
    unsigned i;

    boot[0] = 0xEB;
    boot[1] = 0x3C;
    boot[2] = 0x90;
    memcpy(boot + 3, "MDRMINI ", 8);
    put16(boot + 11, SECTOR);
    boot[13] = 1; /* 每簇扇区数 */
    put16(boot + 14, RESERVED);
    boot[16] = NFAT;
    put16(boot + 17, ROOT_ENTRIES);
    put16(boot + 19, TOTAL_SECTORS);
    boot[21] = 0xF0;
    put16(boot + 22, FAT_SECTORS);
    put16(boot + 24, 18);
    put16(boot + 26, 2);
    boot[38] = 0x29;
    put32(boot + 39, 0x20251022);
    memcpy(boot + 43, "MDRMINI    ", 11);
    memcpy(boot + 54, "FAT12   ", 8);
    boot[510] = 0x55;
    boot[511] = 0xAA;

    fat12_set(fat, 0, 0xFF0);
    fat12_set(fat, 1, 0xFFF);
    fat12_set(fat, 2, 4);
    fat12_set(fat, 4, 0xFFF);
    fat12_set(fat, 5, 7);
    fat12_set(fat, 7, 8);
    fat12_set(fat, 8, 0xFFF);

    dirent(root, "HELLO", "TXT", 0x20, 2, sizeof hello);
    dirent(root + 32, "SYSTEM", "BIN", 0x20, 5, sizeof system);
    dirent(root + 64, "MDRMINI", "", 0x08, 0, 0);

    for (i = 0; i < sizeof hello; i++) {
        hello[i] = (uint8_t)('A' + i % 26);
    }
    for (i = 0; i < sizeof system; i++) {
        system[i] = (uint8_t)((i * 13 + 5) & 0xFF);
    }
    memcpy(cluster_ptr(2), hello, SECTOR);
    memcpy(cluster_ptr(4), hello + SECTOR, sizeof hello - SECTOR);
    memcpy(cluster_ptr(5), system, SECTOR);
    memcpy(cluster_ptr(7), system + SECTOR, SECTOR);
    memcpy(cluster_ptr(8), system + 2 * SECTOR, sizeof system - 2 * SECTOR);

    /* 不在任何簇链上的两簇，内容模仿文件开头 */
    for (i = 0; i < SECTOR; i++) {
        cluster_ptr(3)[i] = (uint8_t)('A' + i % 26);
        cluster_ptr(6)[i] = (uint8_t)((i * 7 + 3) & 0xFF);
    }

    FILE *f = fopen("fat12.img", "wb");
    if (f == NULL) {
        perror("fat12.img");
        return 1;
    }
    fwrite(volume, 1, sizeof volume, f);
    fclose(f);
    printf("fat12.img: %u bytes\n", (unsigned)sizeof volume);
    return 0;
}
