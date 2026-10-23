/* 用 ReadFileEx/WriteFileEx 的完成例程链式复制文件：读完成例程发起写，
   写完成例程发起下一次读。等待用 SleepEx(INFINITE, TRUE)。 */
#include <windows.h>
#include <stdio.h>

#define BUF_SIZE (64 * 1024)

typedef struct copy_state {
    HANDLE src;
    HANDLE dst;
    OVERLAPPED read_ov;
    OVERLAPPED write_ov;
    char buf[BUF_SIZE];
    LONGLONG offset;
    LONGLONG size;
    int reads;
    int writes;
    int failed;
    int finished;
} copy_state;

static copy_state g_state;

static void CALLBACK read_done(DWORD err, DWORD bytes, LPOVERLAPPED ov);
static void CALLBACK write_done(DWORD err, DWORD bytes, LPOVERLAPPED ov);

static void issue_read(copy_state *st) {
    DWORD want;

    if (st->offset >= st->size) {
        st->finished = 1;
        return;
    }
    want = (DWORD)((st->size - st->offset < BUF_SIZE) ? st->size - st->offset
                                                      : BUF_SIZE);
    st->read_ov.Offset = (DWORD)st->offset;
    st->read_ov.OffsetHigh = (DWORD)(st->offset >> 32);
    if (!ReadFileEx(st->src, st->buf, want, &st->read_ov, read_done)) {
        st->failed = (int)GetLastError();
        st->finished = 1;
    }
}

static void CALLBACK read_done(DWORD err, DWORD bytes, LPOVERLAPPED ov) {
    copy_state *st = &g_state;
    LONGLONG at = ((LONGLONG)ov->OffsetHigh << 32) | ov->Offset;

    st->reads++;
    if (err != 0) {
        st->failed = (int)err;
        st->finished = 1;
        return;
    }
    printf("read  #%d: %lu bytes at offset %lld\n", st->reads, bytes,
           (long long)at);
    if (bytes == 0) {
        st->finished = 1;
        return;
    }
    st->write_ov.Offset = ov->Offset;
    st->write_ov.OffsetHigh = ov->OffsetHigh;
    if (!WriteFileEx(st->dst, st->buf, bytes, &st->write_ov, write_done)) {
        st->failed = (int)GetLastError();
        st->finished = 1;
    }
}

static void CALLBACK write_done(DWORD err, DWORD bytes, LPOVERLAPPED ov) {
    copy_state *st = &g_state;
    LONGLONG at = ((LONGLONG)ov->OffsetHigh << 32) | ov->Offset;

    st->writes++;
    if (err != 0) {
        st->failed = (int)err;
        st->finished = 1;
        return;
    }
    printf("write #%d: %lu bytes at offset %lld\n", st->writes, bytes,
           (long long)at);
    st->offset = at + bytes;
    issue_read(st);
}

int wmain(int argc, wchar_t **argv) {
    LARGE_INTEGER size;

    if (argc != 3) {
        printf("usage: readex_copy <source> <destination>\n");
        return 1;
    }
    g_state.src = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (g_state.src == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: %lu\n", GetLastError());
        return 1;
    }
    g_state.dst = CreateFileW(argv[2], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_FLAG_OVERLAPPED, NULL);
    if (g_state.dst == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: %lu\n", GetLastError());
        CloseHandle(g_state.src);
        return 1;
    }
    if (!GetFileSizeEx(g_state.src, &size)) {
        printf("GetFileSizeEx failed: %lu\n", GetLastError());
        CloseHandle(g_state.dst);
        CloseHandle(g_state.src);
        return 1;
    }
    g_state.size = size.QuadPart;
    printf("copying %lld bytes\n", (long long)g_state.size);

    issue_read(&g_state);
    while (!g_state.finished) {
        DWORD rc = SleepEx(INFINITE, TRUE);

        if (rc != WAIT_IO_COMPLETION) {
            printf("SleepEx returned %lu\n", rc);
            break;
        }
    }
    printf("done: %d reads, %d writes", g_state.reads, g_state.writes);
    if (g_state.failed != 0) {
        printf(", failed with error %d", g_state.failed);
    }
    printf("\n");
    CloseHandle(g_state.dst);
    CloseHandle(g_state.src);
    return g_state.failed != 0 ? 1 : 0;
}