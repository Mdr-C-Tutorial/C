#include "cli_parse.h"
#include <stdio.h>
#include <string.h>

void cli_handle_line(const char *line, cli_state *st, char *reply, size_t reply_cap) {
    while (*line == ' ' || *line == '\t') {
        ++line;
    }
    if (*line == '\0') {
        reply[0] = '\0';
        return;
    }

    st->rx_lines++;

    if (strcmp(line, "led on") == 0) {
        st->led_on = 1;
        snprintf(reply, reply_cap, "OK: LED is ON\n");
    } else if (strcmp(line, "led off") == 0) {
        st->led_on = 0;
        snprintf(reply, reply_cap, "OK: LED is OFF\n");
    } else if (strcmp(line, "status") == 0) {
        snprintf(reply, reply_cap, "LED=%s, lines=%lu\n",
                 st->led_on ? "ON" : "OFF",
                 (unsigned long)st->rx_lines);
    } else {
        snprintf(reply, reply_cap, "ERR: unknown command '%s'\n", line);
    }
}
