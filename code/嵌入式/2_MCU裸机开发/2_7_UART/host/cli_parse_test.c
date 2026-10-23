#include <stdio.h>
#include "../cli/cli_parse.h"

int main(void) {
    const char *inputs[] = {
        "led on",
        "status",
        "led off",
        "status",
        "reboot"
    };
    cli_state st = { .led_on = 0, .rx_lines = 0 };
    char reply[96];

    for (size_t i = 0; i < sizeof inputs / sizeof inputs[0]; ++i) {
        cli_handle_line(inputs[i], &st, reply, sizeof reply);
        printf("> %s\n%s", inputs[i], reply);
    }
    return 0;
}
