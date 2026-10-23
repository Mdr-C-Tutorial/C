/* copyfile.c：同一件事交给 CopyFileW。 */
#include <stdio.h>
#include <windows.h>

int wmain(int argc, wchar_t *argv[]) {
    if (argc != 3) {
        printf("usage: copyfile source destination\n");
        return 2;
    }

    if (!CopyFileW(argv[1], argv[2], FALSE)) {
        printf("CopyFileW failed: error %lu\n", GetLastError());
        return 1;
    }

    printf("copied\n");
    return 0;
}