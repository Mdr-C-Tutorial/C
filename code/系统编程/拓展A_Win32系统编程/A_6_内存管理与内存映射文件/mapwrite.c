/* 可写视图：修改写回文件，再用 ReadFile 读回来验证 */
#include <windows.h>

#include <stdio.h>
#include <string.h>

#define FILE_SIZE 4096

int main(void) {
    HANDLE h = CreateFileW(L"greeting.txt", GENERIC_READ | GENERIC_WRITE, 0,
                           NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: %lu\n", (unsigned long)GetLastError());
        return 1;
    }
    /* 空文件没有内容可映射，先把它撑到一页 */
    LARGE_INTEGER size;
    size.QuadPart = FILE_SIZE;
    SetFilePointerEx(h, size, NULL, FILE_BEGIN);
    if (!SetEndOfFile(h)) {
        printf("SetEndOfFile failed: %lu\n", (unsigned long)GetLastError());
        CloseHandle(h);
        return 1;
    }

    HANDLE map = CreateFileMappingW(h, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (map == NULL) {
        printf("CreateFileMappingW failed: %lu\n", (unsigned long)GetLastError());
        CloseHandle(h);
        return 1;
    }
    char *view = MapViewOfFile(map, FILE_MAP_WRITE, 0, 0, 0);
    if (view == NULL) {
        printf("MapViewOfFile failed: %lu\n", (unsigned long)GetLastError());
        CloseHandle(map);
        CloseHandle(h);
        return 1;
    }

    strcpy(view, "hello from the view");
    if (!FlushViewOfFile(view, 0)) {
        printf("FlushViewOfFile failed: %lu\n", (unsigned long)GetLastError());
    }
    UnmapViewOfFile(view);
    CloseHandle(map);
    CloseHandle(h);

    /* 用普通读写读回来，证明修改已经落在文件里 */
    h = CreateFileW(L"greeting.txt", GENERIC_READ, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, 0, NULL);
    char buf[64] = {0};
    DWORD got = 0;
    ReadFile(h, buf, sizeof buf - 1, &got, NULL);
    printf("read back with ReadFile: \"%s\"\n", buf);
    CloseHandle(h);

    /* 只读映射上请求可写视图，创建视图时就失败 */
    h = CreateFileW(L"greeting.txt", GENERIC_READ, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, 0, NULL);
    map = CreateFileMappingW(h, NULL, PAGE_READONLY, 0, 0, NULL);
    char *bad = MapViewOfFile(map, FILE_MAP_WRITE, 0, 0, 0);
    printf("FILE_MAP_WRITE on a PAGE_READONLY mapping: %s, error %lu\n",
           bad == NULL ? "NULL" : "ok", (unsigned long)GetLastError());
    if (bad != NULL) {
        UnmapViewOfFile(bad);
    }
    CloseHandle(map);
    CloseHandle(h);
    return 0;
}