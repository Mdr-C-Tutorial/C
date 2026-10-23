#ifndef BOARD_H
#define BOARD_H

/* 板级配置入口：构建系统用 -DBOARD_HEADER='"board_bluepill.h"' 选择板子。 */
#ifndef BOARD_HEADER
#error "BOARD_HEADER is not defined: pass -DBOARD_HEADER=\"board_bluepill.h\""
#endif

#include BOARD_HEADER

#endif /* BOARD_H */
