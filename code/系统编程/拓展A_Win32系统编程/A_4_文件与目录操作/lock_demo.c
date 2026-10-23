/* lock_demo.c -- 字节范围锁：另一个句柄读被锁住的区域会失败。 */
#include <windows.h>
#include <stdio.h>

static void read_at(HANDLE h, LONGLONG offset, DWORD n) {
    LARGE_INTEGER pos;
    char buf[32];
    DWORD got = 0;

    pos.QuadPart = offset;
    SetFilePointerEx(h, pos, NULL, FILE_BEGIN);
    if (ReadFile(h, buf, n, &got, NULL)) {
        printf("  read %lld-%lld: ok, %lu bytes\n", offset, offset + n - 1, got);
    } else {
        printf("  read %lld-%lld: failed, error %lu\n", offset, offset + n - 1,
               GetLastError());
    }
}

int main(void) {
    HANDLE writer;
    HANDLE reader;
    OVERLAPPED ov = {0};
    DWORD got = 0;
    BOOL ok;

    writer = CreateFileW(L"locked.dat", GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if (writer == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: %lu\n", GetLastError());
        return 1;
    }
    WriteFile(writer, "0123456789abcdef", 16, &got, NULL);
    FlushFileBuffers(writer);

    /* 排他锁住前 4 个字节；FAIL_IMMEDIATELY 让冲突时立刻返回，不排队等待 */
    ok = LockFileEx(writer, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0,
                    4, 0, &ov);
    printf("LockFileEx(0-3, exclusive): %d, error %lu\n", ok,
           ok ? 0ul : GetLastError());

    reader = CreateFileW(L"locked.dat", GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if (reader == INVALID_HANDLE_VALUE) {
        printf("CreateFileW(reader) failed: %lu\n", GetLastError());
        CloseHandle(writer);
        return 1;
    }

    printf("while the range is locked:\n");
    read_at(reader, 0, 4);
    read_at(reader, 4, 4);

    ok = UnlockFileEx(writer, 0, 4, 0, &ov);
    printf("UnlockFileEx(0-3): %d\n", ok);

    printf("after unlock:\n");
    read_at(reader, 0, 4);

    CloseHandle(reader);
    CloseHandle(writer);
    DeleteFileW(L"locked.dat");
    return 0;
}