/* job_kill.exe：父进程把子进程放进 KILL_ON_JOB_CLOSE 的作业对象，
 * 自己一秒钟后退出，子进程（睡 60 秒）随之被结束。 */
#include <locale.h>
#include <stdio.h>
#include <wchar.h>
#include <windows.h>

int wmain(int argc, wchar_t *argv[]) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    HANDLE job = CreateJobObjectW(NULL, NULL);

    if (job == NULL) {
        printf("CreateJobObjectW failed: %lu\n", GetLastError());
        return 1;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit = {0};

    limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limit, sizeof limit)) {
        printf("SetInformationJobObject failed: %lu\n", GetLastError());
        return 1;
    }

    wchar_t cmd[1024];

    if (argc > 1) {
        wcscpy(cmd, argv[1]);
    } else {
        wcscpy(cmd, L"child.exe 0 wait");
    }
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;

    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si,
                        &pi)) {
        printf("CreateProcessW failed: %lu\n", GetLastError());
        return 1;
    }
    if (!AssignProcessToJobObject(job, pi.hProcess)) {
        printf("AssignProcessToJobObject failed: %lu\n", GetLastError());
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }
    ResumeThread(pi.hThread);
    wprintf(L"child pid %lu started in job, parent exits in 1 second\n",
            (unsigned long)pi.dwProcessId);
    wprintf(L"job handle %p\n", (void *)job);
    fflush(stdout);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    Sleep(1000);
    return 0;
}