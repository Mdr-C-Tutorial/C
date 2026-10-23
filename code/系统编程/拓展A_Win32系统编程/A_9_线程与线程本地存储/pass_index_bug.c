/* pass_index_bug.c：把循环对象的地址传给每个线程，四个线程读到的都是同一个值；
 * 改成每个线程一份参数（结构体数组的元素）之后，各自拿到自己的编号。 */
#include <process.h>
#include <stdio.h>
#include <windows.h>

#define N 4

static unsigned __stdcall buggy_worker(void *param) {
    int *index = param;

    Sleep(20); /* 让创建循环先跑完，模拟线程启动的延迟 */
    printf("buggy worker got index %d\n", *index);
    return 0;
}

struct job {
    int index;
};

static unsigned __stdcall fixed_worker(void *param) {
    struct job *job = param;

    printf("fixed worker got index %d\n", job->index);
    return 0;
}

int main(void) {
    HANDLE threads[N];
    struct job jobs[N];

    for (int i = 0; i < N; i++) {
        threads[i] = (HANDLE)_beginthreadex(NULL, 0, buggy_worker, &i, 0, NULL);
        if (threads[i] == NULL) {
            printf("_beginthreadex failed: %lu\n", GetLastError());
            return 1;
        }
    }
    WaitForMultipleObjects(N, threads, TRUE, INFINITE);
    for (int i = 0; i < N; i++) {
        CloseHandle(threads[i]);
    }

    printf("---\n");
    for (int i = 0; i < N; i++) {
        jobs[i].index = i;
        threads[i] = (HANDLE)_beginthreadex(NULL, 0, fixed_worker, &jobs[i], 0, NULL);
        if (threads[i] == NULL) {
            printf("_beginthreadex failed: %lu\n", GetLastError());
            return 1;
        }
    }
    WaitForMultipleObjects(N, threads, TRUE, INFINITE);
    for (int i = 0; i < N; i++) {
        CloseHandle(threads[i]);
    }
    return 0;
}