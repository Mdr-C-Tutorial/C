#include <stdio.h>

__declspec(dllexport) const char *plugin_name(void) {
    return "double";
}

__declspec(dllexport) int plugin_run(int x) {
    return x * 2;
}