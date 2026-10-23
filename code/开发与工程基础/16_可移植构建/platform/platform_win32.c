#include "platform.h"

#include <io.h>
#include <stdio.h>

int wf_stdout_is_terminal(void) { return _isatty(_fileno(stdout)) != 0; }
