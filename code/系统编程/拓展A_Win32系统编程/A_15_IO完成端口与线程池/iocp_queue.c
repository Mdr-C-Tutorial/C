/* 把 I/O 完成端口当作线程安全的工作队列：投递 8 个任务包，3 个工作线程处理，
 * 8 个任务全部处理完后投递 3 个完成键为 0 的退出包。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>

#define WORKERS 3
#define TASKS 8

struct task {
    int id;
};

static struct task g_tasks[TASKS];
static volatile LONG g_done;
static HANDLE g_all_done;

static DWORD WINAPI worker(LPVOID arg) {
    HANDLE port = (HANDLE)arg;
    DWORD count;
    ULONG_PTR key;
    OVERLAPPED *ov;

    for (;;) {
        if (!GetQueuedCompletionStatus(port, &count, &key, &ov, INFINITE)) {
            printf("worker %lu: GetQueuedCompletionStatus failed: %lu\n",
                   GetCurrentThreadId(), GetLastError());
            return 1;
        }
        if (key == 0) {
            printf("worker %lu: 收到退出包\n", GetCurrentThreadId());
            return 0;
        }
        struct task *t = (struct task *)key;
        printf("worker %lu: 处理任务 %d（包内数值 %lu）\n", GetCurrentThreadId(),
               t->id, count);
        Sleep(30);
        if (InterlockedIncrement(&g_done) == TASKS) {
            SetEvent(g_all_done);
        }
    }
}

int main(void) {
    HANDLE port;
    HANDLE threads[WORKERS];
    OVERLAPPED dummy = {0};

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (port == NULL) {
        printf("CreateIoCompletionPort failed: %lu\n", GetLastError());
        return 1;
    }
    g_all_done = CreateEventW(NULL, TRUE, FALSE, NULL);

    for (int i = 0; i < WORKERS; i++) {
        threads[i] = CreateThread(NULL, 0, worker, port, 0, NULL);
    }

    for (int i = 0; i < TASKS; i++) {
        g_tasks[i].id = i;
        PostQueuedCompletionStatus(port, (DWORD)(i + 1),
                                   (ULONG_PTR)&g_tasks[i], NULL);
    }
    printf("main: 已投递 %d 个任务包\n", TASKS);

    WaitForSingleObject(g_all_done, INFINITE);
    printf("main: 全部任务处理完毕，投递 %d 个退出包\n", WORKERS);
    for (int i = 0; i < WORKERS; i++) {
        PostQueuedCompletionStatus(port, 0, 0, &dummy);
    }

    WaitForMultipleObjects(WORKERS, threads, TRUE, INFINITE);
    for (int i = 0; i < WORKERS; i++) {
        CloseHandle(threads[i]);
    }
    CloseHandle(g_all_done);
    CloseHandle(port);
    return 0;
}