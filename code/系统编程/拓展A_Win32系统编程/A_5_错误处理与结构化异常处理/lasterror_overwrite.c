#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

/* 模拟一个日志函数：内部会调用 API，从而改写调用线程的最后错误码 */
static void log_msg(const char *msg)
{
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);

    printf("[log] %s\n", msg);
    CloseHandle(ev);
}

static HANDLE try_open_missing(void)
{
    return CreateFileW(L"no_such_file.txt", GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

int main(void)
{
    DWORD code;
    HANDLE h;

    /* 错误写法：失败与取码之间夹了别的 API 调用 */
    h = try_open_missing();
    if (h == INVALID_HANDLE_VALUE) {
        log_msg("open failed");
        code = GetLastError();
        printf("late GetLastError = %lu  (want 2)\n", (unsigned long)code);
    }

    /* 正确写法：一失败就取 */
    h = try_open_missing();
    if (h == INVALID_HANDLE_VALUE) {
        code = GetLastError();
        log_msg("open failed");
        printf("early GetLastError = %lu\n", (unsigned long)code);
    }
    return 0;
}