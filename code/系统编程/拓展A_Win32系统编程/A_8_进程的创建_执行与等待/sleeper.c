/* sleeper.exe：睡 60 秒，供 job_kill.exe 演示作业对象结束子进程。 */
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

int main(int argc, char *argv[]) {
    unsigned seconds = 60;

    if (argc > 1) {
        seconds = (unsigned)atoi(argv[1]);
    }
    printf("sleeper %lu: sleeping %u s\n", (unsigned long)GetCurrentProcessId(), seconds);
    fflush(stdout);
    Sleep(seconds * 1000);
    printf("sleeper %lu: woke up\n", (unsigned long)GetCurrentProcessId());
    return 0;
}