#ifndef CLI_PARSE_H
#define CLI_PARSE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int led_on;
    uint32_t rx_lines;
} cli_state;

void cli_handle_line(const char *line, cli_state *st, char *reply, size_t reply_cap);

#endif /* CLI_PARSE_H */
