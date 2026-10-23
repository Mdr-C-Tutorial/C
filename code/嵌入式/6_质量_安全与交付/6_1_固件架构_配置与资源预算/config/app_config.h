#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "board.h"

/* 功能开关与缓冲区大小。构建系统可以用 -D 覆盖这里的默认值。 */
#ifndef APP_USE_W25Q64
#define APP_USE_W25Q64 BOARD_HAS_W25Q64
#endif

#ifndef APP_LOG_BUF_SIZE
#define APP_LOG_BUF_SIZE 256u
#endif

#ifndef APP_LINE_MAX
#define APP_LINE_MAX 128u
#endif

#if APP_USE_W25Q64 && !BOARD_HAS_W25Q64
#error "APP_USE_W25Q64=1 but this board has no external flash"
#endif

_Static_assert((APP_LOG_BUF_SIZE & (APP_LOG_BUF_SIZE - 1u)) == 0u,
               "APP_LOG_BUF_SIZE must be a power of two");
_Static_assert(APP_LOG_BUF_SIZE >= APP_LINE_MAX,
               "APP_LOG_BUF_SIZE must hold at least one command line");
_Static_assert(BOARD_LED_PIN < 16u, "BOARD_LED_PIN out of range");

#endif /* APP_CONFIG_H */
