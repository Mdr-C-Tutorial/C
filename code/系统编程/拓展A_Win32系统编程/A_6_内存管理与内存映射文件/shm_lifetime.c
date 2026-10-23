/* 命名映射对象的生存期：句柄关闭之后视图仍有效，但名字已经打不开 */
#include <windows.h>

#include <stdio.h>
#include <string.h>

#define SHM_SIZE 4096
#define SHM_NAME L"Local\\MdrTutorial.Lifetime"

int main(void) {
    HANDLE map = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                    0, SHM_SIZE, SHM_NAME);
    DWORD err = GetLastError();
    if (map == NULL) {
        printf("CreateFileMappingW failed: %lu\n", (unsigned long)err);
        return 1;
    }
    printf("create #1: GetLastError = %lu (183 means the name existed)\n",
           (unsigned long)err);

    char *view = MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    if (view == NULL) {
        printf("MapViewOfFile failed: %lu\n", (unsigned long)GetLastError());
        CloseHandle(map);
        return 1;
    }
    strcpy(view, "the view still works");

    CloseHandle(map);
    printf("closed the mapping handle; the view still reads: \"%s\"\n", view);

    HANDLE by_name = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, SHM_NAME);
    printf("OpenFileMappingW by name while only a view is alive: %s, error %lu\n",
           by_name == NULL ? "NULL" : "handle", (unsigned long)GetLastError());
    if (by_name != NULL) {
        CloseHandle(by_name);
    }

    HANDLE again = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                      0, SHM_SIZE, SHM_NAME);
    err = GetLastError();
    printf("create #2 with the same name: %s, GetLastError = %lu\n",
           again == NULL ? "NULL" : "handle", (unsigned long)err);
    if (again != NULL) {
        char *v2 = MapViewOfFile(again, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
        if (v2 != NULL) {
            printf("the new object's view reads: \"%s\" (starts out zeroed)\n",
                   v2);
            UnmapViewOfFile(v2);
        }
        CloseHandle(again);
    }

    UnmapViewOfFile(view);
    HANDLE gone = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, SHM_NAME);
    printf("after every handle and view is closed: %s, error %lu\n",
           gone == NULL ? "NULL" : "handle", (unsigned long)GetLastError());
    if (gone != NULL) {
        CloseHandle(gone);
    }
    return 0;
}