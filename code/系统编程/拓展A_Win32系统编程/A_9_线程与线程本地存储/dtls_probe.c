/* dtls_probe.c：MinGW 的 GCC 不认识 __declspec(thread)，只给一条
 * -Wattributes 警告然后忽略它。工作线程改了值，主线程读到的也是改过的值，
 * 说明这个对象根本不是每线程一份。 */
#include <process.h>
#include <stdio.h>
#include <windows.h>

static __declspec(thread) int g_counter = 5;

static unsigned __stdcall worker(void *param) {
    (void)param;
    printf("worker: __declspec(thread) = %d\n", g_counter);
    g_counter = 99;
    return 0;
}

int main(void) {
    HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, worker, NULL, 0, NULL);

    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    printf("main: __declspec(thread) = %d\n", g_counter);
    return 0;
}