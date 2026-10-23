/* records.c -- 定长记录文件：读取、就地修改第三条记录、截断与读到末尾。 */
#include <windows.h>
#include <stdio.h>

#define RECORD_SIZE 64
#define N_RECORDS 5

typedef struct {
    int id;
    char name[48];
    double score;
} record;

_Static_assert(sizeof(record) == RECORD_SIZE, "record must be exactly 64 bytes");

static BOOL read_record(HANDLE h, int index, record *rec) {
    LARGE_INTEGER off;
    DWORD got = 0;

    off.QuadPart = (LONGLONG)index * RECORD_SIZE;
    if (!SetFilePointerEx(h, off, NULL, FILE_BEGIN)) {
        return FALSE;
    }
    return ReadFile(h, rec, sizeof *rec, &got, NULL) && got == sizeof *rec;
}

static BOOL write_record(HANDLE h, int index, const record *rec) {
    LARGE_INTEGER off;
    DWORD got = 0;

    off.QuadPart = (LONGLONG)index * RECORD_SIZE;
    if (!SetFilePointerEx(h, off, NULL, FILE_BEGIN)) {
        return FALSE;
    }
    return WriteFile(h, rec, sizeof *rec, &got, NULL) && got == sizeof *rec;
}

int main(void) {
    record rec;
    HANDLE h;
    LARGE_INTEGER size;
    LARGE_INTEGER off;
    DWORD got = 0;
    BOOL ok;
    int i;

    h = CreateFileW(L"records.dat", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                    NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: %lu\n", GetLastError());
        return 1;
    }

    for (i = 0; i < N_RECORDS; i++) {
        ZeroMemory(&rec, sizeof rec);
        rec.id = i + 1;
        snprintf(rec.name, sizeof rec.name, "student-%02d", i + 1);
        rec.score = 60.0 + i * 5;
        if (!write_record(h, i, &rec)) {
            printf("write record %d failed: %lu\n", i + 1, GetLastError());
            CloseHandle(h);
            return 1;
        }
    }

    /* 随机访问：只读出第三条，改一个字段，再写回去 */
    if (!read_record(h, 2, &rec)) {
        printf("read record 3 failed: %lu\n", GetLastError());
        CloseHandle(h);
        return 1;
    }
    printf("record 3 before: id=%d name=%s score=%.1f\n", rec.id, rec.name,
           rec.score);
    rec.score = 99.5;
    if (!write_record(h, 2, &rec)) {
        printf("rewrite record 3 failed: %lu\n", GetLastError());
        CloseHandle(h);
        return 1;
    }
    printf("record 3 updated\n\n");

    for (i = 0; i < N_RECORDS; i++) {
        if (!read_record(h, i, &rec)) {
            break;
        }
        printf("  %d: %-12s %6.1f\n", rec.id, rec.name, rec.score);
    }

    /* 再读一条：文件已经结束，同步句柄返回 TRUE 且 0 字节 */
    SetFilePointerEx(h, (LARGE_INTEGER){0}, NULL, FILE_END);
    ok = ReadFile(h, &rec, sizeof rec, &got, NULL);
    printf("\nReadFile at end: ok=%d, %lu bytes\n", ok, got);

    /* 截断到 3 条记录：先定位，再 SetEndOfFile */
    off.QuadPart = 3 * (LONGLONG)RECORD_SIZE;
    SetFilePointerEx(h, off, NULL, FILE_BEGIN);
    ok = SetEndOfFile(h);
    GetFileSizeEx(h, &size);
    printf("SetEndOfFile at 3 records: ok=%d, size %lld bytes\n", ok,
           (long long)size.QuadPart);

    CloseHandle(h);
    return 0;
}