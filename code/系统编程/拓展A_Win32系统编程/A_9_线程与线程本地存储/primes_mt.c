/* primes_mt.c：把 [2, LIMIT) 均分成若干段，每个线程统计自己那一段里的素数个数，
 * 主线程汇总并用 QueryPerformanceCounter 计时。
 *
 *   primes_mt.exe <线程数>
 */
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define LIMIT 20000000ULL
#define MAX_THREADS 16

struct task {
    unsigned long long lo; /* 含 */
    unsigned long long hi; /* 不含 */
    unsigned long long count;
};

static unsigned long long g_primes[4096]; /* sqrt(LIMIT) 以内的素数，只读共享 */
static int g_nprimes;

static void build_base_primes(void) {
    for (unsigned long long n = 2; n * n < LIMIT; n++) {
        int prime = 1;

        for (int i = 0; i < g_nprimes; i++) {
            if (g_primes[i] * g_primes[i] > n) {
                break;
            }
            if (n % g_primes[i] == 0) {
                prime = 0;
                break;
            }
        }
        if (prime) {
            g_primes[g_nprimes++] = n;
        }
    }
}

static int is_prime(unsigned long long n) {
    if (n < 2) {
        return 0;
    }
    for (int i = 0; i < g_nprimes; i++) {
        if (g_primes[i] * g_primes[i] > n) {
            return 1;
        }
        if (n % g_primes[i] == 0) {
            return 0;
        }
    }
    return 1;
}

static unsigned __stdcall count_primes(void *param) {
    struct task *task = param;

    for (unsigned long long n = task->lo; n < task->hi; n++) {
        task->count += (unsigned long long)is_prime(n);
    }
    return 0;
}

int main(int argc, char **argv) {
    LARGE_INTEGER freq;
    LARGE_INTEGER start;
    LARGE_INTEGER end;
    struct task tasks[MAX_THREADS];
    HANDLE threads[MAX_THREADS];
    unsigned long long total = 0;
    unsigned long long chunk;
    int nthreads = 4;

    if (argc > 1) {
        nthreads = atoi(argv[1]);
    }
    if (nthreads < 1 || nthreads > MAX_THREADS) {
        printf("thread count must be 1..%d\n", MAX_THREADS);
        return 1;
    }

    build_base_primes();
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    chunk = (LIMIT - 2 + nthreads - 1) / (unsigned long long)nthreads;
    for (int i = 0; i < nthreads; i++) {
        tasks[i].lo = 2 + chunk * (unsigned long long)i;
        tasks[i].hi = tasks[i].lo + chunk;
        if (tasks[i].hi > LIMIT) {
            tasks[i].hi = LIMIT;
        }
        tasks[i].count = 0;
        threads[i] = (HANDLE)_beginthreadex(NULL, 0, count_primes, &tasks[i], 0, NULL);
        if (threads[i] == NULL) {
            printf("_beginthreadex failed: %lu\n", GetLastError());
            return 1;
        }
    }
    WaitForMultipleObjects((DWORD)nthreads, threads, TRUE, INFINITE);
    for (int i = 0; i < nthreads; i++) {
        CloseHandle(threads[i]);
    }
    QueryPerformanceCounter(&end);

    for (int i = 0; i < nthreads; i++) {
        total += tasks[i].count;
    }
    printf("%d thread(s): %llu primes below %llu, %.3f s\n", nthreads, total,
           (unsigned long long)LIMIT,
           (double)(end.QuadPart - start.QuadPart) / (double)freq.QuadPart);
    return 0;
}