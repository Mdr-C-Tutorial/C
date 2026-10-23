#include "ff.h"
#include "diskio.h"
#include <string.h>

#define RAM_SECTOR_SIZE  512u
#define RAM_SECTOR_COUNT 256u

static BYTE g_ram_disk[RAM_SECTOR_COUNT * RAM_SECTOR_SIZE];
static DSTATUS g_stat = STA_NOINIT;

DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0) {
        return STA_NOINIT;
    }
    g_stat = 0;
    return g_stat;
}

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0) {
        return STA_NOINIT;
    }
    return g_stat;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || (g_stat & STA_NOINIT) != 0) {
        return RES_NOTRDY;
    }
    if (sector + count > RAM_SECTOR_COUNT) {
        return RES_PARERR;
    }
    memcpy(buff, &g_ram_disk[sector * RAM_SECTOR_SIZE],
           count * RAM_SECTOR_SIZE);
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || (g_stat & STA_NOINIT) != 0) {
        return RES_NOTRDY;
    }
    if (sector + count > RAM_SECTOR_COUNT) {
        return RES_PARERR;
    }
    memcpy(&g_ram_disk[sector * RAM_SECTOR_SIZE], buff,
           count * RAM_SECTOR_SIZE);
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv != 0 || (g_stat & STA_NOINIT) != 0) {
        return RES_NOTRDY;
    }
    switch (cmd) {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_COUNT:
        *(LBA_t *)buff = RAM_SECTOR_COUNT;
        return RES_OK;
    case GET_SECTOR_SIZE:
        *(WORD *)buff = RAM_SECTOR_SIZE;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 1;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}

DWORD get_fattime(void) {
    /* 固定返回 2026-10-18 12:00:00 的 FAT 时间戳 */
    return ((DWORD)(2026 - 1980) << 25) |
           ((DWORD)10 << 21) |
           ((DWORD)18 << 16) |
           ((DWORD)12 << 11) |
           ((DWORD)0 << 5) |
           ((DWORD)0 >> 1);
}
