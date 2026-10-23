#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#define LWIP_PLATFORM_DIAG(x)                                                  \
    do {                                                                       \
        printf x;                                                              \
    } while (0)

#define LWIP_PLATFORM_ASSERT(x)                                                \
    do {                                                                       \
        printf("Assertion \"%s\" failed at %s:%d\n", x, __FILE__, __LINE__);   \
        abort();                                                               \
    } while (0)

#define LWIP_RAND() ((u32_t)rand())

#endif /* LWIP_ARCH_CC_H */
