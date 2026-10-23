#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <locale.h>
#include <stdio.h>

/* VEH：进程级，先于任何帧上的处理程序运行 */
static LONG CALLBACK veh_handler(PEXCEPTION_POINTERS info)
{
    EXCEPTION_RECORD *rec = info->ExceptionRecord;

    printf("VEH: code=0x%08lX address=%p\n", (unsigned long)rec->ExceptionCode,
           rec->ExceptionAddress);
    return EXCEPTION_CONTINUE_SEARCH; /* 交回正常的分发流程 */
}

/* 未处理异常筛选器：最后机会，写 dump */
static LONG WINAPI crash_filter(PEXCEPTION_POINTERS info)
{
    MINIDUMP_EXCEPTION_INFORMATION mei;
    HANDLE file;
    BOOL ok;

    printf("unhandled filter: code=0x%08lX\n",
           (unsigned long)info->ExceptionRecord->ExceptionCode);

    file = CreateFileW(L"crash.dmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: %lu\n", (unsigned long)GetLastError());
        return EXCEPTION_EXECUTE_HANDLER;
    }

    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = info;
    mei.ClientPointers = FALSE;

    ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                           MiniDumpNormal, &mei, NULL, NULL);
    printf("MiniDumpWriteDump: %s\n", ok ? "ok" : "failed");
    CloseHandle(file);
    return EXCEPTION_EXECUTE_HANDLER; /* 处理完即终止进程 */
}

int main(void)
{
    int *p = NULL;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0); /* 崩溃时缓冲区里的内容会丢 */
    DeleteFileW(L"crash.dmp");

    AddVectoredExceptionHandler(1, veh_handler);
    SetUnhandledExceptionFilter(crash_filter);

    printf("about to dereference NULL\n");
    printf("%d\n", *p); /* 这里崩溃 */
    return 0;
}