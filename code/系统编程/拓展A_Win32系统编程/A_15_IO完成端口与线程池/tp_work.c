/* 线程池工作对象：提交 10 个工作项，主线程等待全部完成。
 * 工作项打印执行它的线程 ID，可以看出线程被复用。 */
#include <stdio.h>
#include <windows.h>

#define ITEMS 10

struct item {
    PTP_WORK work;
    int id;
};

static volatile LONG g_done;

static VOID CALLBACK work_cb(PTP_CALLBACK_INSTANCE instance, PVOID context,
                             PTP_WORK work) {
    struct item *it = (struct item *)context;

    (void)instance;
    (void)work;
    printf("工作项 %2d：线程 %lu\n", it->id, GetCurrentThreadId());
    Sleep(50);
    InterlockedIncrement(&g_done);
}

int main(void) {
    struct item items[ITEMS];

    for (int i = 0; i < ITEMS; i++) {
        items[i].id = i;
        items[i].work = CreateThreadpoolWork(work_cb, &items[i], NULL);
        if (items[i].work == NULL) {
            printf("CreateThreadpoolWork 失败: %lu\n", GetLastError());
            return 1;
        }
        SubmitThreadpoolWork(items[i].work);
    }

    for (int i = 0; i < ITEMS; i++) {
        WaitForThreadpoolWorkCallbacks(items[i].work, FALSE);
        CloseThreadpoolWork(items[i].work);
    }
    printf("全部完成：%ld 个\n", (long)g_done);
    return 0;
}