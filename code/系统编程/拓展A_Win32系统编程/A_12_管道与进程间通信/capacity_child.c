/* 每次写 1 字节，最多 1 MiB，用来量管道能积压多少字节。 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    char byte = 'x';
    unsigned long long n = 0;

    while (n < 1024 * 1024) {
        DWORD written = 0;
        if (!WriteFile(out, &byte, 1, &written, NULL)) {
            break;
        }
        n++;
    }
    return 0;
}
