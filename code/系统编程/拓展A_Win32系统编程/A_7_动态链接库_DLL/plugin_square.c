#include <stdio.h>

__declspec(dllexport) const char *plugin_name(void) {
    return "square";
}

__declspec(dllexport) int plugin_run(int x) {
    return x * x;
}