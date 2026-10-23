#ifndef APP_BOARD_H
#define APP_BOARD_H

#include <stdbool.h>

void board_init(void);
void board_led_set(bool on);
bool board_button_pressed(void);
void board_console_puts(const char *s);

#endif
