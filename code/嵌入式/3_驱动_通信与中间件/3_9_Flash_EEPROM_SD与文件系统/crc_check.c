/* 打印 CRC-32/ISO-HDLC 的标准验证值，用于核对 flash_cfg.c 的实现 */
#include "flash_cfg.h"
#include <stdio.h>

int main(void) {
    printf("crc32(\"123456789\") = 0x%08X\n",
           (unsigned)crc32_calc("123456789", 9u));
    return 0;
}