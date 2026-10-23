/* busy.exe：空转约 150 毫秒，用来观察 GetProcessTimes 的用户态时间。 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    ULONGLONG start = GetTickCount64();
    unsigned long long acc = 0;

    while (GetTickCount64() - start < 150) {
        for (int i = 0; i < 10000; i++) {
            acc += (unsigned long long)i;
        }
    }
    printf("busy done, acc = %llu\n", acc);
    return 0;
}