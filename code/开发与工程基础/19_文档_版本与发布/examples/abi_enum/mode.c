#include "wf/mode.h"

#include <stdio.h>

int wf_passes(enum wf_mode mode) {
    switch (mode) {
    case WF_MODE_COMPACT:
        puts("compact: 3 passes");
        return 3;
    case WF_MODE_FAST:
        puts("fast: 1 pass");
        return 1;
    case WF_MODE_EXACT:
        puts("exact: check every byte");
        return 2;
    }
    return -1;
}
