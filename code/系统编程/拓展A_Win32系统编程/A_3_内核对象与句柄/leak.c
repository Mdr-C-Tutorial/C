/* leak.c：每轮都漏一个句柄；带参数 fix 时每轮关闭，句柄数不再增长。 */
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define REQUESTS 10000

static DWORD handle_count(void) {
    DWORD n = 0;

    if (!GetProcessHandleCount(GetCurrentProcess(), &n)) {
        printf("GetProcessHandleCount failed: %lu\n", GetLastError());
    }
    return n;
}

/* 每处理一个「请求」创建一个事件。正常代码在这里等待结束再关闭；
   下面这版把 CloseHandle(done) 漏掉了。 */
static void handle_request(int n, int fix) {
    HANDLE done = CreateEventW(NULL, FALSE, FALSE, NULL);

    if (done == NULL) {
        printf("CreateEventW failed: %lu\n", GetLastError());
        return;
    }
    (void)n;
    if (fix) {
        CloseHandle(done);
    }
}

int main(int argc, char *argv[]) {
    int fix = argc > 1 && strcmp(argv[1], "fix") == 0;
    DWORD base = handle_count();
    int i;

    printf("mode: %s, start: %lu handles\n",
           fix ? "CloseHandle every request" : "never close", base);
    for (i = 1; i <= REQUESTS; i++) {
        handle_request(i, fix);
        if (i % 2500 == 0) {
            DWORD now = handle_count();
            printf("after %d requests: +%lu\n", i, now - base);
        }
    }
    printf("end: +%lu handles\n", handle_count() - base);
    return 0;
}