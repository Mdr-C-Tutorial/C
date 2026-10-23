#ifndef FIFO_REC_H
#define FIFO_REC_H

#include <stddef.h>

/* 记录格式（定长 len 字节，最后一个是换行）：
 *   NN:SSSS:XXXX…X
 * NN 是两位客户端编号，SSSS 是四位序号，冒号之后全部填字母 'A' + NN。
 * 记录完整时不依赖任何定界符就能按长度切开。 */

static int record_ok(const char *p, size_t len) {
    if (len < 9 || p[len - 1] != '\n') {
        return 0;
    }
    if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9') {
        return 0;
    }
    if (p[2] != ':' || p[7] != ':') {
        return 0;
    }
    for (int k = 3; k < 7; k++) {
        if (p[k] < '0' || p[k] > '9') {
            return 0;
        }
    }
    char want = (char)('A' + (p[0] - '0') * 10 + (p[1] - '0'));
    for (size_t k = 8; k + 1 < len; k++) {
        if (p[k] != want) {
            return 0;
        }
    }
    return 1;
}

#endif
