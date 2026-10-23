/* fileinfo.c -- 打印文件大小、属性位与三个时间，并把 FILETIME 换算成 Unix 时间。 */
#include <windows.h>
#include <stdio.h>
#include <locale.h>

/* FILETIME 是自 1601-01-01 UTC 起的 100 ns 计数；Unix 时间从 1970-01-01 起算，
   两者相差 116444736000000000 个 100 ns 单位。 */
#define FILETIME_UNIX_EPOCH 116444736000000000ULL

static void show_attributes(DWORD attr) {
    static const struct {
        DWORD bit;
        const char *name;
    } flags[] = {
        {FILE_ATTRIBUTE_READONLY, "READONLY"},
        {FILE_ATTRIBUTE_HIDDEN, "HIDDEN"},
        {FILE_ATTRIBUTE_SYSTEM, "SYSTEM"},
        {FILE_ATTRIBUTE_DIRECTORY, "DIRECTORY"},
        {FILE_ATTRIBUTE_ARCHIVE, "ARCHIVE"},
    };
    size_t i;

    printf("0x%08lX", (unsigned long)attr);
    for (i = 0; i < sizeof flags / sizeof flags[0]; i++) {
        if (attr & flags[i].bit) {
            printf(" %s", flags[i].name);
        }
    }
    printf("\n");
}

static void show_time(const char *label, const FILETIME *ft) {
    SYSTEMTIME utc;
    SYSTEMTIME local;
    ULARGE_INTEGER v;

    if (!FileTimeToSystemTime(ft, &utc)) {
        printf("%-10s  (cannot convert)\n", label);
        return;
    }
    /* NTFS 存的是 UTC；转本地时间要经过当前时区与夏令时规则 */
    if (!SystemTimeToTzSpecificLocalTime(NULL, &utc, &local)) {
        local = utc;
    }
    v.LowPart = ft->dwLowDateTime;
    v.HighPart = ft->dwHighDateTime;
    printf("%-10s  %04u-%02u-%02u %02u:%02u:%02u  unix %llu\n", label, local.wYear,
           local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond,
           (unsigned long long)((v.QuadPart - FILETIME_UNIX_EPOCH) / 10000000ULL));
}

int wmain(int argc, wchar_t **argv) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    ULARGE_INTEGER size;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (argc != 2) {
        printf("usage: fileinfo <file>\n");
        return 1;
    }
    if (!GetFileAttributesExW(argv[1], GetFileExInfoStandard, &fad)) {
        printf("GetFileAttributesExW failed: %lu\n", GetLastError());
        return 1;
    }

    size.LowPart = fad.nFileSizeLow;
    size.HighPart = fad.nFileSizeHigh;
    printf("size        %llu bytes\n", (unsigned long long)size.QuadPart);
    printf("attributes  ");
    show_attributes(fad.dwFileAttributes);
    show_time("created", &fad.ftCreationTime);
    show_time("accessed", &fad.ftLastAccessTime);
    show_time("written", &fad.ftLastWriteTime);
    return 0;
}