#include "diff_util.h"
#include "mdr_memory.h"

#include <stdio.h>
#include <string.h>

/* 16 字节缓冲区上的重叠复制穷举：目标偏移、源偏移、字节数各取 0–15，
 * 要求两个区域都落在缓冲区内。参照是标准库 memmove，
 * 比较整块 16 字节，包括复制范围之外不应改动的部分。 */
int main(void) {
    for (size_t dest = 0; dest < 16; dest++) {
        for (size_t src = 0; src < 16; src++) {
            for (size_t count = 0; count < 16; count++) {
                unsigned char a[16];
                unsigned char b[16];

                if (dest + count > 16 || src + count > 16) {
                    continue;
                }
                for (size_t i = 0; i < 16; i++) {
                    a[i] = (unsigned char)(i + 1u);
                    b[i] = a[i];
                }
                memmove(a + dest, a + src, count);
                mdr_memmove(b + dest, b + src, count);
                if (memcmp(a, b, 16) != 0) {
                    diff_report("memmove", b, 16, (long)dest, (long)src);
                } else {
                    diff_ok();
                }
            }
        }
    }
    return diff_summary("diff_memmove");
}