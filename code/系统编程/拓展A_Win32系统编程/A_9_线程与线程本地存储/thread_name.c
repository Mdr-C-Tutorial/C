/* thread_name.c：用 GetProcAddress 取得 SetThreadDescription 与 GetThreadDescription，
 * 给主线程和一个工作线程命名，再读回名字。
 * 两个函数从 Windows 10 1607 起提供，MinGW 的头文件里没有声明。 */
#include <process.h>
#include <stdio.h>
#include <windows.h>

typedef HRESULT(WINAPI *set_thread_description_fn)(HANDLE, PCWSTR);
typedef HRESULT(WINAPI *get_thread_description_fn)(HANDLE, PWSTR *);

static set_thread_description_fn g_set_thread_description;
static get_thread_description_fn g_get_thread_description;

static unsigned __stdcall worker(void *param) {
    HRESULT hr;

    (void)param;
    hr = g_set_thread_description(GetCurrentThread(), L"mdr-worker");
    printf("worker: SetThreadDescription -> 0x%08lX (SUCCEEDED: %d)\n",
           (unsigned long)hr, SUCCEEDED(hr));
    return 0;
}

static void print_name(HANDLE thread, const char *who) {
    PWSTR name = NULL;
    HRESULT hr = g_get_thread_description(thread, &name);

    if (SUCCEEDED(hr) && name != NULL) {
        printf("%s: name is \"%ls\"\n", who, name);
        LocalFree(name);
    } else {
        printf("%s: GetThreadDescription -> 0x%08lX\n", who, (unsigned long)hr);
    }
}

int main(void) {
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    HANDLE thread;
    HRESULT hr;

    g_set_thread_description =
        (set_thread_description_fn)(void (*)(void))GetProcAddress(
            kernel32, "SetThreadDescription");
    g_get_thread_description =
        (get_thread_description_fn)(void (*)(void))GetProcAddress(
            kernel32, "GetThreadDescription");
    if (g_set_thread_description == NULL || g_get_thread_description == NULL) {
        printf("thread description APIs not available (error %lu)\n",
               GetLastError());
        return 1;
    }

    hr = g_set_thread_description(GetCurrentThread(), L"mdr-main");
    printf("main: SetThreadDescription -> 0x%08lX (SUCCEEDED: %d)\n",
           (unsigned long)hr, SUCCEEDED(hr));
    print_name(GetCurrentThread(), "main");

    thread = (HANDLE)_beginthreadex(NULL, 0, worker, NULL, 0, NULL);
    if (thread == NULL) {
        printf("_beginthreadex failed: %lu\n", GetLastError());
        return 1;
    }
    WaitForSingleObject(thread, INFINITE);
    print_name(thread, "worker");
    CloseHandle(thread);
    return 0;
}