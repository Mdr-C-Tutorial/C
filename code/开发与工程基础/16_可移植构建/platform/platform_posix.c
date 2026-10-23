#define _POSIX_C_SOURCE 200809L

#include "platform.h"

#include <unistd.h>

int wf_stdout_is_terminal(void) { return isatty(STDOUT_FILENO) != 0; }
