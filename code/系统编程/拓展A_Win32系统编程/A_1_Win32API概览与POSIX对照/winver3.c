/* winver3.c：版本宏决定 HPCON 类型是否可见。 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    HPCON pc = NULL;
    printf("_WIN32_WINNT = 0x%04X\n", (unsigned)_WIN32_WINNT);
    printf("HPCON size = %llu\n", (unsigned long long)sizeof pc);
    return 0;
}