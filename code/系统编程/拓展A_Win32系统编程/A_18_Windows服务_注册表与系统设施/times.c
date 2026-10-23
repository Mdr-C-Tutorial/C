/* 三种时间来源：实时时钟、单调计数器、开机以来的毫秒数。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -o times.exe times.c
 */
#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <wchar.h>

/* FILETIME 以 100 纳秒为单位，起点是 1601-01-01 UTC；减去 1970 年之前的秒数得到 Unix 时间。 */
static double filetime_to_unix_seconds(FILETIME ft) {
    ULARGE_INTEGER v;
    v.LowPart = ft.dwLowDateTime;
    v.HighPart = ft.dwHighDateTime;
    return (double)v.QuadPart / 1.0e7 - 11644473600.0;
}

static void show_system_time(void) {
    FILETIME ft;
    SYSTEMTIME st;
    ULARGE_INTEGER v;

    GetSystemTimePreciseAsFileTime(&ft);
    v.LowPart = ft.dwLowDateTime;
    v.HighPart = ft.dwHighDateTime;
    FileTimeToSystemTime(&ft, &st);
    wprintf(L"GetSystemTimePreciseAsFileTime: %04u-%02u-%02u %02u:%02u:%02u.%03u UTC\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    wprintf(L"  as Unix seconds: %.7f\n", filetime_to_unix_seconds(ft));
    wprintf(L"  raw 100 ns ticks: %llu\n", (unsigned long long)v.QuadPart);
}

static void show_counter(void) {
    LARGE_INTEGER freq;
    LARGE_INTEGER a;
    LARGE_INTEGER b;
    LARGE_INTEGER c;
    LONGLONG min_delta = -1;
    int i;

    QueryPerformanceFrequency(&freq);
    wprintf(L"QueryPerformanceFrequency: %lld Hz\n", (long long)freq.QuadPart);

    /* 反复读，找出相邻两次读数的最小非零差值，即计数器的实际分辨率。 */
    QueryPerformanceCounter(&a);
    for (i = 0; i < 20000; i++) {
        QueryPerformanceCounter(&b);
        LONGLONG d = b.QuadPart - a.QuadPart;
        if (d > 0 && (min_delta < 0 || d < min_delta)) {
            min_delta = d;
        }
        a = b;
    }
    wprintf(L"smallest nonzero step: %lld ticks (%.3f us)\n", (long long)min_delta,
            (double)min_delta * 1.0e6 / (double)freq.QuadPart);

    QueryPerformanceCounter(&b);
    Sleep(250);
    QueryPerformanceCounter(&c);
    wprintf(L"Sleep(250) measured: %.3f ms\n",
            (double)(c.QuadPart - b.QuadPart) * 1000.0 / (double)freq.QuadPart);
}

static void show_tick_count(void) {
    ULONGLONG a;
    ULONGLONG b;

    a = GetTickCount64();
    Sleep(120);
    b = GetTickCount64();
    wprintf(L"GetTickCount64: %llu -> %llu, delta %llu ms\n", (unsigned long long)a,
            (unsigned long long)b, (unsigned long long)(b - a));
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    show_system_time();
    show_counter();
    show_tick_count();
    return 0;
}