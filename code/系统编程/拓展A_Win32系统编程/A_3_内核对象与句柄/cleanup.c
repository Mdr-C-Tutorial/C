/* cleanup.c：用 goto 统一清理，句柄关闭后置 NULL，句柄数回到起点。 */
#include <stdio.h>
#include <windows.h>

static DWORD handle_count(void) {
    DWORD n = 0;

    if (!GetProcessHandleCount(GetCurrentProcess(), &n)) {
        printf("GetProcessHandleCount failed: %lu\n", GetLastError());
    }
    return n;
}

/* 打开两个对象；path 为 NULL 时模拟中途失败。 */
static int process(const wchar_t *path) {
    HANDLE file = INVALID_HANDLE_VALUE;
    HANDLE event = NULL;
    int rc = 1;

    printf("  before: %lu handles\n", handle_count());

    event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (event == NULL) {
        printf("  CreateEventW failed: %lu\n", GetLastError());
        goto cleanup;
    }

    if (path == NULL) {
        printf("  simulated failure after the event was created\n");
        goto cleanup;
    }

    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        printf("  CreateFileW failed: %lu\n", GetLastError());
        goto cleanup;
    }

    printf("  work done on the file\n");
    rc = 0;

cleanup:
    /* 逆序释放，每关一个就置空，避免重复关闭。 */
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
        file = INVALID_HANDLE_VALUE;
    }
    if (event != NULL) {
        CloseHandle(event);
        event = NULL;
    }
    printf("  after:  %lu handles\n", handle_count());
    return rc;
}

int main(void) {
    printf("normal path:\n");
    process(L"cleanup.c");

    printf("failure path:\n");
    process(NULL);

    printf("end: %lu handles\n", handle_count());
    return 0;
}