/* 系统信息：页面大小、分配粒度、处理器数、物理内存、版本号。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -o sysinfo.exe sysinfo.c
 */
#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <wchar.h>

/* RtlGetVersion 由 ntdll 导出，头文件不声明；winternl.h 提供 RTL_OSVERSIONINFOW。 */
typedef LONG(WINAPI *rtl_get_version_fn)(PRTL_OSVERSIONINFOW);

static void show_system_info(void) {
    SYSTEM_INFO si;
    SYSTEM_INFO native;

    GetSystemInfo(&si);
    GetNativeSystemInfo(&native);
    wprintf(L"processor architecture : %u\n", (unsigned)si.wProcessorArchitecture);
    wprintf(L"processors             : %lu logical, %lu native\n",
            (unsigned long)si.dwNumberOfProcessors, (unsigned long)native.dwNumberOfProcessors);
    wprintf(L"page size              : %lu bytes\n", (unsigned long)si.dwPageSize);
    wprintf(L"allocation granularity : %lu bytes\n", (unsigned long)si.dwAllocationGranularity);
    wprintf(L"min/max app address    : %p / %p\n", si.lpMinimumApplicationAddress,
            si.lpMaximumApplicationAddress);
}

static void show_memory(void) {
    MEMORYSTATUSEX ms = {.dwLength = sizeof ms};

    if (!GlobalMemoryStatusEx(&ms)) {
        wprintf(L"GlobalMemoryStatusEx failed: %lu\n", (unsigned long)GetLastError());
        return;
    }
    wprintf(L"memory load            : %lu%%\n", (unsigned long)ms.dwMemoryLoad);
    wprintf(L"physical memory        : %.1f GiB total, %.1f GiB available\n",
            (double)ms.ullTotalPhys / (1024.0 * 1024.0 * 1024.0),
            (double)ms.ullAvailPhys / (1024.0 * 1024.0 * 1024.0));
    wprintf(L"commit limit           : %.1f GiB, available %.1f GiB\n",
            (double)ms.ullTotalPageFile / (1024.0 * 1024.0 * 1024.0),
            (double)ms.ullAvailPageFile / (1024.0 * 1024.0 * 1024.0));
}

static void show_version(void) {
    OSVERSIONINFOW legacy = {.dwOSVersionInfoSize = sizeof legacy};
    RTL_OSVERSIONINFOW current = {.dwOSVersionInfoSize = sizeof current};
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    rtl_get_version_fn fn =
        ntdll != NULL
            ? (rtl_get_version_fn)(void (*)(void))GetProcAddress(ntdll, "RtlGetVersion")
            : NULL;

    if (GetVersionExW(&legacy)) {
        wprintf(L"GetVersionExW          : %lu.%lu build %lu\n",
                (unsigned long)legacy.dwMajorVersion, (unsigned long)legacy.dwMinorVersion,
                (unsigned long)legacy.dwBuildNumber);
    }
    if (fn != NULL && fn(&current) == 0) {
        wprintf(L"RtlGetVersion          : %lu.%lu build %lu\n",
                (unsigned long)current.dwMajorVersion, (unsigned long)current.dwMinorVersion,
                (unsigned long)current.dwBuildNumber);
    }
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    show_system_info();
    show_memory();
    show_version();
    return 0;
}