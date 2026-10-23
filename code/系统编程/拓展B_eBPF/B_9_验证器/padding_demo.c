#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct record {
    uint32_t pid;
    uint64_t ts;
};

int main(void)
{
    struct record rec;
    const unsigned char *p = (const unsigned char *)&rec;
    size_t i;

    memset(&rec, 0xAA, sizeof rec);
    rec.pid = 0x11223344u;
    rec.ts = 0x0102030405060708ull;

    printf("sizeof(struct record) = %lu\n", (unsigned long)sizeof rec);
    printf("offsetof(pid) = %lu, offsetof(ts) = %lu\n",
           (unsigned long)offsetof(struct record, pid),
           (unsigned long)offsetof(struct record, ts));
    for (i = 0; i < sizeof rec; i++) {
        printf("%02x%c", p[i], (i + 1) % 8 == 0 ? '\n' : ' ');
    }
    return 0;
}