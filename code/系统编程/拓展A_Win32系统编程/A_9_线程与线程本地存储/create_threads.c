/* create_threads.c：同一个求和任务分别交给 CreateThread 与 _beginthreadex 创建的线程，
 * 等待结束后取回退出码；最后演示线程还在运行时 GetExitCodeThread 返回 STILL_ACTIVE。 */
#include <process.h>
#include <stdio.h>
#include <windows.h>

#define LIMIT 100000

struct job {
    int limit;
    unsigned long long sum;
};

static DWORD WINAPI sum_with_create_thread(LPVOID param) {
    struct job *job = param;
    unsigned long long sum = 0;

    for (int i = 1; i <= job->limit; i++) {
        sum += (unsigned long long)i;
    }
    job->sum = sum;
    return (DWORD)sum;
}

static unsigned __stdcall sum_with_beginthreadex(void *param) {
    struct job *job = param;
    unsigned long long sum = 0;

    for (int i = 1; i <= job->limit; i++) {
        sum += (unsigned long long)i;
    }
    job->sum = sum;
    return (unsigned)sum;
}

static DWORD WINAPI sleeper(LPVOID param) {
    (void)param;
    Sleep(150);
    return 7;
}

int main(void) {
    struct job a = {.limit = LIMIT};
    struct job b = {.limit = LIMIT};
    HANDLE threads[2];
    DWORD exit_code = 0;
    HANDLE s;

    threads[0] = CreateThread(NULL, 0, sum_with_create_thread, &a, 0, NULL);
    if (threads[0] == NULL) {
        printf("CreateThread failed: %lu\n", GetLastError());
        return 1;
    }
    threads[1] = (HANDLE)_beginthreadex(NULL, 0, sum_with_beginthreadex, &b, 0, NULL);
    if (threads[1] == NULL) {
        printf("_beginthreadex failed: %lu\n", GetLastError());
        CloseHandle(threads[0]);
        return 1;
    }

    printf("CreateThread:   handle %p, thread id %lu\n", (void *)threads[0],
           (unsigned long)GetThreadId(threads[0]));
    printf("_beginthreadex: handle %p, thread id %lu\n", (void *)threads[1],
           (unsigned long)GetThreadId(threads[1]));

    WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    GetExitCodeThread(threads[0], &exit_code);
    printf("CreateThread:   sum = %llu, exit code %lu (0x%08lX)\n", a.sum,
           (unsigned long)exit_code, (unsigned long)exit_code);
    GetExitCodeThread(threads[1], &exit_code);
    printf("_beginthreadex: sum = %llu, exit code %lu (0x%08lX)\n", b.sum,
           (unsigned long)exit_code, (unsigned long)exit_code);
    CloseHandle(threads[0]);
    CloseHandle(threads[1]);

    /* 线程还没结束时，退出码是 STILL_ACTIVE，它的值恰好等于 259 */
    s = CreateThread(NULL, 0, sleeper, NULL, 0, NULL);
    if (s == NULL) {
        printf("CreateThread failed: %lu\n", GetLastError());
        return 1;
    }
    GetExitCodeThread(s, &exit_code);
    printf("running thread: exit code %lu (STILL_ACTIVE is %lu)\n",
           (unsigned long)exit_code, (unsigned long)STILL_ACTIVE);
    WaitForSingleObject(s, INFINITE);
    GetExitCodeThread(s, &exit_code);
    printf("after wait:     exit code %lu\n", (unsigned long)exit_code);
    CloseHandle(s);
    return 0;
}