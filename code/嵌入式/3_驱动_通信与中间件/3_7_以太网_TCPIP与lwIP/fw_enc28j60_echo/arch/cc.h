#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>

#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#define LWIP_PLATFORM_DIAG(x)                                                  \
    do {                                                                       \
        (void)0;                                                               \
    } while (0)

#define LWIP_PLATFORM_ASSERT(x)                                                \
    do {                                                                       \
        (void)(x);                                                             \
        for (;;) {                                                             \
        }                                                                      \
    } while (0)

uint32_t lwip_port_rand(void);
#define LWIP_RAND() ((u32_t)lwip_port_rand())

#endif /* LWIP_ARCH_CC_H */
