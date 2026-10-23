/* Sleep 的实际粒度：请求的毫秒数只是下限，实际返回时刻受系统时钟节拍限制。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -o sleep_grain.exe sleep_grain.c
 */
#include <windows.h>
#include <stdio.h>

static double now_ms(LARGE_INTEGER freq) {
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)freq.QuadPart;
}

int main(void) {
    const DWORD requests[] = {0, 1, 5, 10, 50, 100};
    LARGE_INTEGER freq;
    int i;
    int round;

    QueryPerformanceFrequency(&freq);
    for (i = 0; i < (int)(sizeof requests / sizeof requests[0]); i++) {
        double sum = 0.0;
        double min = 1.0e9;
        double max = 0.0;

        for (round = 0; round < 10; round++) {
            double begin = now_ms(freq);
            double elapsed;

            Sleep(requests[i]);
            elapsed = now_ms(freq) - begin;
            sum += elapsed;
            if (elapsed < min) {
                min = elapsed;
            }
            if (elapsed > max) {
                max = elapsed;
            }
        }
        printf("Sleep(%3lu): avg %7.2f ms, min %7.2f, max %7.2f\n",
               (unsigned long)requests[i], sum / 10.0, min, max);
    }
    return 0;
}