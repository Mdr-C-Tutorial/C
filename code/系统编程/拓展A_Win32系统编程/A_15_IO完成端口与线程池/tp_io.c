/* 线程池 I/O 对象：异步读一个文件。
 * 每次发起 ReadFile 之前调用 StartThreadpoolIo；发起失败时调用 CancelThreadpoolIo。 */
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static volatile LONG g_done = 0;
static HANDLE g_event;
static char g_buf[4096];

static VOID CALLBACK io_cb(PTP_CALLBACK_INSTANCE instance, PVOID context,
                           PVOID overlapped, ULONG io_result, ULONG_PTR nbytes,
                           PTP_IO io) {
    (void)instance;
    (void)context;
    (void)overlapped;
    (void)io;

    if (io_result == NO_ERROR) {
        g_buf[nbytes < sizeof g_buf ? nbytes : sizeof g_buf - 1] = '\0';
        char *nl = strchr(g_buf, '\n');
        if (nl != NULL) {
            *nl = '\0';
        }
        printf("读取完成：%llu 字节，首行：%s\n", (unsigned long long)nbytes,
               g_buf);
    } else {
        printf("读取失败：错误 %lu\n", (unsigned long)io_result);
    }
    InterlockedIncrement(&g_done);
    SetEvent(g_event);
}

/* 先写一个内容固定的文件，保证输出不随源文件变化。 */
static int make_input(const wchar_t *path) {
    static const char text[] = "第一行：线程池 I/O\n第二行：StartThreadpoolIo\n"
                               "第三行：完成回调\n";
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written = 0;
    BOOL ok;

    if (f == INVALID_HANDLE_VALUE) {
        return -1;
    }
    ok = WriteFile(f, text, (DWORD)(sizeof text - 1), &written, NULL);
    CloseHandle(f);
    return ok && written == sizeof text - 1 ? 0 : -1;
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (make_input(L"tp_io_demo.txt") != 0) {
        printf("写测试文件失败: %lu\n", GetLastError());
        return 1;
    }

    HANDLE f = CreateFileW(L"tp_io_demo.txt", GENERIC_READ, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        printf("CreateFileW 失败: %lu\n", GetLastError());
        return 1;
    }

    g_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    PTP_IO io = CreateThreadpoolIo(f, io_cb, NULL, NULL);
    if (io == NULL) {
        printf("CreateThreadpoolIo 失败: %lu\n", GetLastError());
        return 1;
    }

    OVERLAPPED ov = {0};
    StartThreadpoolIo(io);
    if (!ReadFile(f, g_buf, sizeof g_buf - 1, NULL, &ov)) {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING) {
            WaitForSingleObject(g_event, 5000);
        } else {
            /* 发起就失败了，要撤销刚才的通知 */
            CancelThreadpoolIo(io);
            printf("ReadFile 失败: %lu\n", err);
        }
    } else {
        /* 操作立即成功时完成通知照样会到达 */
        WaitForSingleObject(g_event, 5000);
    }

    WaitForThreadpoolIoCallbacks(io, TRUE);
    CloseThreadpoolIo(io);
    CloseHandle(g_event);
    CloseHandle(f);
    printf("结束，回调 %ld 次\n", (long)g_done);
    return 0;
}