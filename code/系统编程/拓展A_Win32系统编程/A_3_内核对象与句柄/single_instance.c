/* single_instance.c：用命名互斥量保证同一会话里只运行一个实例。 */
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define INSTANCE_NAME L"Local\\MdrTutorial.SingleInstance"

int main(int argc, char *argv[]) {
    int hold = 3;

    if (argc > 1) {
        hold = atoi(argv[1]);
    }
    if (hold < 0) {
        hold = 0;
    }

    HANDLE mutex = CreateMutexW(NULL, FALSE, INSTANCE_NAME);
    if (mutex == NULL) {
        printf("CreateMutexW failed: %lu\n", GetLastError());
        return 2;
    }

    /* 创建成功也可能带 ERROR_ALREADY_EXISTS：名字已被占用，拿到的是已有对象的句柄。 */
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        printf("another instance is running, this one exits (pid %lu)\n", GetCurrentProcessId());
        CloseHandle(mutex);
        return 1;
    }

    printf("running as the only instance (pid %lu) for %d s\n", GetCurrentProcessId(), hold);
    Sleep((DWORD)hold * 1000);
    printf("the only instance exits\n");

    /* 最后一个句柄关闭时，对象随之销毁。 */
    CloseHandle(mutex);
    return 0;
}