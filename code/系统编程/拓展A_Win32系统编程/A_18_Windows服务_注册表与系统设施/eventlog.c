/* 向事件日志写一条记录。
 *
 * 事件源不必事先注册：RegisterEventSourceW 找不到源时会退回 Application 日志，
 * 事件照常写入，只是事件查看器里看不到描述文字（没有消息文件）。
 * 要带上描述，需要在安装时把消息文件登记到
 * HKLM\SYSTEM\CurrentControlSet\Services\EventLog\Application\<源名>，这一步需要管理员权限。
 *
 * 本示例只编译，不运行：运行会往本机事件日志写入测试记录。
 * 构建：gcc -std=c17 -Wall -Wextra -o eventlog.exe eventlog.c
 */
#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <wchar.h>

int main(void) {
    const wchar_t *source = L"MdrTutorial";
    const wchar_t *strings[] = {L"MdrEcho started on port 23456.",
                                L"MdrEcho stopped."};
    HANDLE log;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    log = RegisterEventSourceW(NULL, source);
    if (log == NULL) {
        wprintf(L"RegisterEventSourceW failed: %lu\n", (unsigned long)GetLastError());
        return 1;
    }

    if (!ReportEventW(log, EVENTLOG_INFORMATION_TYPE, 0, 1000, NULL, 2, 0, strings, NULL)) {
        wprintf(L"ReportEventW failed: %lu\n", (unsigned long)GetLastError());
        DeregisterEventSource(log);
        return 1;
    }
    wprintf(L"two information events written to the Application log under source %ls\n", source);

    DeregisterEventSource(log);
    return 0;
}