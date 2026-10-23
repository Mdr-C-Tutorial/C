#include "ff.h"
#include <stdio.h>
#include <string.h>

static BYTE g_work[FF_MAX_SS];

int main(void) {
    FATFS fs;
    FIL fil;
    UINT bw = 0;
    UINT br = 0;

    /* 1. 在内存盘上创建无分区表的 FAT12 卷（FM_FAT | FM_SFD） */
    MKFS_PARM opt = {
        .fmt = FM_FAT | FM_SFD,
        .n_fat = 1,
        .align = 0,
        .n_root = 64,
        .au_size = 512,
    };
    FRESULT res = f_mkfs("", &opt, g_work, sizeof(g_work));
    printf("f_mkfs -> %d\n", (int)res);

    /* 2. 立即挂载卷 */
    res = f_mount(&fs, "", 1);
    printf("f_mount -> %d\n", (int)res);

    /* 3. 创建并写入 CSV 日志，调用 f_sync 刷回目录项与 FAT 表 */
    res = f_open(&fil, "LOG.CSV", FA_CREATE_ALWAYS | FA_WRITE);
    const char *csv = "ts_ms,temp_c\n1000,25.4\n2000,25.6\n";
    f_write(&fil, csv, (UINT)strlen(csv), &bw);
    f_sync(&fil);
    f_close(&fil);
    printf("wrote %u bytes to LOG.CSV\n", (unsigned)bw);

    /* 4. 重新打开并读回验证 */
    char buf[128] = {0};
    res = f_open(&fil, "LOG.CSV", FA_READ);
    f_read(&fil, buf, sizeof(buf) - 1u, &br);
    f_close(&fil);
    printf("read %u bytes:\n%s", (unsigned)br, buf);

    f_unmount("");
    return 0;
}
