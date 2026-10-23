/* disposition.c -- 五种创建处置对同一个文件的行为，以及成功时也可能改写最后错误码。 */
#include <windows.h>
#include <stdio.h>

static void probe(int exists, DWORD disposition, const char *name) {
    HANDLE h;

    DeleteFileW(L"probe.dat");
    if (exists) {
        h = CreateFileW(L"probe.dat", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);
        CloseHandle(h);
    }

    SetLastError(0);
    h = CreateFileW(L"probe.dat", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                    disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("%-18s %-12s failed   error %lu\n", name,
               exists ? "existing" : "missing", GetLastError());
    } else {
        printf("%-18s %-12s opened   last error %lu\n", name,
               exists ? "existing" : "missing", GetLastError());
        CloseHandle(h);
    }
}

int main(void) {
    printf("%-18s %-12s %s\n", "disposition", "file", "result");
    probe(0, CREATE_NEW, "CREATE_NEW");
    probe(0, CREATE_ALWAYS, "CREATE_ALWAYS");
    probe(0, OPEN_EXISTING, "OPEN_EXISTING");
    probe(0, OPEN_ALWAYS, "OPEN_ALWAYS");
    probe(0, TRUNCATE_EXISTING, "TRUNCATE_EXISTING");
    printf("\n");
    probe(1, CREATE_NEW, "CREATE_NEW");
    probe(1, CREATE_ALWAYS, "CREATE_ALWAYS");
    probe(1, OPEN_EXISTING, "OPEN_EXISTING");
    probe(1, OPEN_ALWAYS, "OPEN_ALWAYS");
    probe(1, TRUNCATE_EXISTING, "TRUNCATE_EXISTING");

    DeleteFileW(L"probe.dat");
    return 0;
}