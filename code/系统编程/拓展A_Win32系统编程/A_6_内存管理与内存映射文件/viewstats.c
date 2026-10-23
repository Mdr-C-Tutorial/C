/* 一个 1 GiB 的只读视图对提交量和工作集的影响：viewstats.exe <file> */
#include <windows.h>
#include <psapi.h>

#include <stdio.h>
#include <wchar.h>

static void show(const char *label) {
    PROCESS_MEMORY_COUNTERS pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc);
    printf("%-30s PagefileUsage %8llu KiB  WorkingSetSize %8llu KiB\n", label,
           (unsigned long long)(pmc.PagefileUsage / 1024),
           (unsigned long long)(pmc.WorkingSetSize / 1024));
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: viewstats <file>\n");
        return 2;
    }
    show("start");

    wchar_t path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, path, MAX_PATH);

    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: %lu\n", (unsigned long)GetLastError());
        return 1;
    }
    HANDLE map = CreateFileMappingW(h, NULL, PAGE_READONLY, 0, 0, NULL);
    if (map == NULL) {
        printf("CreateFileMappingW failed: %lu\n",
               (unsigned long)GetLastError());
        CloseHandle(h);
        return 1;
    }
    char *view = MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
    if (view == NULL) {
        printf("MapViewOfFile failed: %lu\n", (unsigned long)GetLastError());
        CloseHandle(map);
        CloseHandle(h);
        return 1;
    }
    LARGE_INTEGER size;
    GetFileSizeEx(h, &size);
    show("after mapping the whole file");

    /* 每 4 KiB 碰一次，把整个视图的页都拉进工作集 */
    unsigned long long sum = 0;
    for (long long i = 0; i < size.QuadPart; i += 4096) {
        sum += (unsigned char)view[i];
    }
    printf("sum of the first byte of every page: %llu\n", sum);
    show("after touching every page");

    UnmapViewOfFile(view);
    CloseHandle(map);
    CloseHandle(h);
    show("after unmapping");
    return 0;
}