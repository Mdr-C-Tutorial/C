/* 命名共享内存的写方：创建页面文件支持的映射，每秒更新一次计数 */
#include <windows.h>

#include <stdio.h>

#define SHM_SIZE 4096
#define SHM_NAME L"Local\\MdrTutorial.Shm"

struct shared_block {
    volatile LONG counter;
    volatile LONG ticks;
    char message[64];
};

int main(void) {
    HANDLE map = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                    0, SHM_SIZE, SHM_NAME);
    if (map == NULL) {
        printf("CreateFileMappingW failed: %lu\n", (unsigned long)GetLastError());
        return 1;
    }
    printf("created %ls (error %lu; 183 would mean it already existed)\n",
           SHM_NAME, (unsigned long)GetLastError());

    struct shared_block *shm =
        MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    if (shm == NULL) {
        printf("MapViewOfFile failed: %lu\n", (unsigned long)GetLastError());
        CloseHandle(map);
        return 1;
    }

    shm->counter = 0;
    shm->ticks = 0;
    snprintf(shm->message, sizeof shm->message, "written by pid %lu",
             (unsigned long)GetCurrentProcessId());
    printf("view at %p, message: \"%s\"\n", (void *)shm, shm->message);

    for (int i = 0; i < 10; i++) {
        Sleep(300);
        InterlockedIncrement(&shm->counter);
        shm->ticks = i + 1;
        printf("writer: counter = %ld\n", (long)shm->counter);
    }

    UnmapViewOfFile(shm);
    CloseHandle(map);
    printf("writer done, all handles closed\n");
    return 0;
}