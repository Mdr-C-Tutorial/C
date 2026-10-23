/* tls_demo.c：每个线程都要记录自己的错误消息，对比四种存放方式：
 * 一个所有线程共享的全局缓冲区、静态 TLS（_Thread_local）、
 * 动态 TLS（TlsAlloc）与 FLS（FlsAlloc，带回调）。 */
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define N_THREADS 3

static char g_shared_error[64];               /* 所有线程共用，写错的做法 */
static _Thread_local char g_static_error[64]; /* 静态 TLS，每个线程一份 */

static DWORD g_tls_index; /* 动态 TLS 的索引，全进程共用一个 */
static DWORD g_fls_index; /* FLS 的索引，线程结束时调用登记的回调 */

static LONG g_callback_count;

static VOID WINAPI fls_cleanup(PVOID value) {
    InterlockedIncrement(&g_callback_count);
    printf("  fls callback: thread %lu frees \"%s\"\n", GetCurrentThreadId(),
           (const char *)value);
    free(value);
}

static unsigned __stdcall worker(void *param) {
    int id = *(int *)param;
    char *tls_block;
    char *fls_block;

    snprintf(g_shared_error, sizeof g_shared_error, "thread %d failed", id);
    snprintf(g_static_error, sizeof g_static_error, "thread %d failed", id);

    tls_block = malloc(64);
    fls_block = malloc(64);
    if (tls_block == NULL || fls_block == NULL) {
        free(tls_block);
        free(fls_block);
        return 1;
    }
    snprintf(tls_block, 64, "thread %d failed", id);
    TlsSetValue(g_tls_index, tls_block);
    snprintf(fls_block, 64, "thread %d failed", id);
    FlsSetValue(g_fls_index, fls_block);

    /* 让其他线程有机会跑完，再回头读四种存放方式 */
    Sleep(100 + id);
    printf("thread %d: shared \"%s\", static TLS \"%s\", dynamic TLS \"%s\", "
           "FLS \"%s\"\n",
           id, g_shared_error, g_static_error,
           (const char *)TlsGetValue(g_tls_index),
           (const char *)FlsGetValue(g_fls_index));

    /* 动态 TLS 的槽要自己释放，系统不会替你调用 free */
    free(TlsGetValue(g_tls_index));
    TlsSetValue(g_tls_index, NULL);
    return 0;
}

int main(void) {
    HANDLE threads[N_THREADS];
    int ids[N_THREADS];

    g_tls_index = TlsAlloc();
    if (g_tls_index == TLS_OUT_OF_INDEXES) {
        printf("TlsAlloc failed: %lu\n", GetLastError());
        return 1;
    }
    g_fls_index = FlsAlloc(fls_cleanup);
    if (g_fls_index == FLS_OUT_OF_INDEXES) {
        printf("FlsAlloc failed: %lu\n", GetLastError());
        TlsFree(g_tls_index);
        return 1;
    }
    printf("TLS index = %lu, FLS index = %lu\n", (unsigned long)g_tls_index,
           (unsigned long)g_fls_index);

    for (int i = 0; i < N_THREADS; i++) {
        ids[i] = i;
        threads[i] = (HANDLE)_beginthreadex(NULL, 0, worker, &ids[i], 0, NULL);
        if (threads[i] == NULL) {
            printf("_beginthreadex failed: %lu\n", GetLastError());
            FlsFree(g_fls_index);
            TlsFree(g_tls_index);
            return 1;
        }
    }
    WaitForMultipleObjects(N_THREADS, threads, TRUE, INFINITE);
    for (int i = 0; i < N_THREADS; i++) {
        CloseHandle(threads[i]);
    }

    printf("main: static TLS \"%s\" (its own zero-initialized copy)\n",
           g_static_error);
    printf("main: FLS callback ran %ld time(s)\n", (long)g_callback_count);

    FlsFree(g_fls_index);
    TlsFree(g_tls_index);
    return 0;
}