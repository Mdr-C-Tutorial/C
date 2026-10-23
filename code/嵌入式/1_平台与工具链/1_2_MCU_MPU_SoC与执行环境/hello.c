#include <stdint.h>
#include <stdio.h>

uint32_t g_boot_magic = 0x103C8000u;
uint32_t g_sample_buf[16];

static uint32_t fill_and_sum(uint32_t seed) {
    uint32_t sum = 0;
    for (size_t i = 0; i < 16; ++i) {
        g_sample_buf[i] = seed + (uint32_t)i;
        sum += g_sample_buf[i];
    }
    return sum;
}

int main(void) {
    uint32_t total = fill_and_sum(g_boot_magic);
    printf("sample[0] = 0x%08lX, sum = 0x%08lX\n",
           (unsigned long)g_sample_buf[0],
           (unsigned long)total);
    return 0;
}
