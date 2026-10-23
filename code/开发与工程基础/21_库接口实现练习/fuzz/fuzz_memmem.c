#include "mdr_ext.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* libFuzzer 差分目标：同一段输入切成 haystack 与 needle 两半，
 * 交给 mdr_memmem 与逐位置比较的参照实现，比较命中的偏移。 */

static const void *ref_memmem(const void *haystack, size_t haystacklen,
                              const void *needle, size_t needlelen) {
    const unsigned char *h = (const unsigned char *)haystack;

    if (needlelen == 0) {
        return (void *)haystack;
    }
    if (haystacklen < needlelen) {
        return NULL;
    }
    for (size_t i = 0; i + needlelen <= haystacklen; i++) {
        if (memcmp(h + i, needle, needlelen) == 0) {
            return (void *)(h + i);
        }
    }
    return NULL;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    size_t haylen;
    size_t neelen;
    const void *got;
    const void *want;
    long got_off;
    long want_off;

    if (size < 2) {
        return 0;
    }
    haylen = size / 2;
    neelen = size - haylen;
    got = mdr_memmem(data, haylen, data + haylen, neelen);
    want = ref_memmem(data, haylen, data + haylen, neelen);
    got_off = got == NULL ? -1 : (long)((const uint8_t *)got - data);
    want_off = want == NULL ? -1 : (long)((const uint8_t *)want - data);
    if (got_off != want_off) {
        fprintf(stderr, "memmem mismatch: hay %zu bytes, needle %zu bytes, "
                        "want offset %ld, got %ld\n",
                haylen, neelen, want_off, got_off);
        abort();
    }
    return 0;
}