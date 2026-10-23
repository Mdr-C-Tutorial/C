#include <windows.h>

static int g_count = 0;

__declspec(dllexport) int counter_bump(void) {
    return ++g_count;
}